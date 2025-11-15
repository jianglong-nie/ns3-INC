/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology: 8 spine 8 leaf with JSON configuration
// - Parse job configuration from JSON files
// - Configure routing based on ARO (Aggregation Routing Optimization) JSON
// - Support multiple jobs with different parameter servers

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/atp-module.h"
#include "ns3/ptr.h"
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <vector>
using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-SpineLeaf-JSON");

// 网络拓扑结构
struct NetworkTopology {
    static const uint32_t nSpines = 8;
    static const uint32_t nLeaves = 8; 
    static const uint32_t nServersPerLeaf = 8;
    static const uint32_t nTotalServers = nLeaves * nServersPerLeaf;
    static const uint32_t nTotalNodes = nSpines + nLeaves + nTotalServers;
    
    // 节点索引分配
    static const uint32_t spineStartIdx = 0;
    static const uint32_t leafStartIdx = spineStartIdx + nSpines;
    static const uint32_t serverStartIdx = leafStartIdx + nLeaves;
};

// Job配置结构
struct JobConfig {
    uint32_t id;
    std::vector<std::string> workers;
    std::string ps;
    std::string ps_leaf;
};

// 路由配置结构 (ARO)
struct RouteConfig {
    std::map<uint32_t, std::vector<std::pair<std::string, std::string>>> jobRoutes;
};

// 节点名称到索引的映射
std::map<std::string, uint32_t> nodeNameToIndex;
std::map<uint32_t, std::string> nodeIndexToName;

// 全局变量用于跟踪
std::ofstream cwndStream_job1, cwndStream_job2;
std::ofstream SinkBytesStream_job1, SinkBytesStream_job2;
uint64_t lastTimeJob1Bytes = 0, lastTimeJob2Bytes = 0;

// 简单的JSON解析辅助函数
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\"");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r\"");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> parseStringArray(const std::string& arrayStr) {
    std::vector<std::string> result;
    std::string content = arrayStr;
    
    // 移除方括号
    size_t start = content.find('[');
    size_t end = content.find_last_of(']');
    if (start != std::string::npos && end != std::string::npos) {
        content = content.substr(start + 1, end - start - 1);
    }
    
    // 按逗号分割
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

// 解析Jobs配置文件
std::vector<JobConfig> parseJobsConfig(const std::string& filename) {
    std::vector<JobConfig> jobs;
    std::ifstream file(filename);
    if (!file.is_open()) {
        NS_FATAL_ERROR("Cannot open jobs config file: " << filename);
    }
    
    std::string line;
    JobConfig currentJob;
    bool inJob = false;
    bool inWorkers = false;
    std::string workersContent;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        if (line.find("\"id\":") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string idStr = line.substr(pos + 1);
                idStr = trim(idStr);
                if (idStr.back() == ',') idStr.pop_back();
                currentJob.id = std::stoi(idStr);
                inJob = true;
            }
        }
        else if (line.find("\"workers\":") != std::string::npos) {
            inWorkers = true;
            size_t pos = line.find('[');
            if (pos != std::string::npos) {
                workersContent = line.substr(pos);
            }
        }
        else if (inWorkers && line.find(']') != std::string::npos) {
            if (workersContent.find(']') == std::string::npos) {
                workersContent += line;
            }
            currentJob.workers = parseStringArray(workersContent);
            inWorkers = false;
            workersContent.clear();
        }
        else if (inWorkers) {
            workersContent += line;
        }
        else if (line.find("\"ps\":") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string psStr = line.substr(pos + 1);
                psStr = trim(psStr);
                if (psStr.back() == ',') psStr.pop_back();
                currentJob.ps = psStr;
            }
        }
        else if (line.find("\"ps_leaf\":") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string psLeafStr = line.substr(pos + 1);
                psLeafStr = trim(psLeafStr);
                if (psLeafStr.back() == ',') psLeafStr.pop_back();
                currentJob.ps_leaf = psLeafStr;
            }
        }
        else if (line == "}" && inJob) {
            jobs.push_back(currentJob);
            currentJob = JobConfig();
            inJob = false;
        }
    }
    
    return jobs;
}

RouteConfig parseRouteConfig(const std::string& filename) {
    RouteConfig config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        NS_FATAL_ERROR("Cannot open route config file: " << filename);
    }
    
    std::string line;
    uint32_t currentJobId = 0;
    bool inJobRoutes = false;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // 查找job ID
        if (line.find("\"1\":") != std::string::npos) {
            currentJobId = 1;
            inJobRoutes = true;
        }
        else if (line.find("\"2\":") != std::string::npos) {
            currentJobId = 2;
            inJobRoutes = true;
        }
        else if (inJobRoutes && line.find('[') != std::string::npos && line.find(']') != std::string::npos) {
            // 解析路由对 ["from", "to"]
            size_t start = line.find('[');
            size_t end = line.find(']');
            if (start != std::string::npos && end != std::string::npos) {
                std::string routeStr = line.substr(start + 1, end - start - 1);
                std::vector<std::string> routePair = parseStringArray("[" + routeStr + "]");
                if (routePair.size() == 2) {
                    config.jobRoutes[currentJobId].push_back({routePair[0], routePair[1]});
                }
            }
        }
        else if (line == "]" && inJobRoutes) {
            inJobRoutes = false;
        }
    }
    
    return config;
}

// 初始化节点名称映射
void initializeNodeMapping() {
    // Spine节点: spine1-spine8
    for (uint32_t i = 0; i < NetworkTopology::nSpines; i++) {
        std::string name = "spine" + std::to_string(i + 1);
        uint32_t index = NetworkTopology::spineStartIdx + i;
        nodeNameToIndex[name] = index;
        nodeIndexToName[index] = name;
    }
    
    // Leaf节点: leaf1-leaf8
    for (uint32_t i = 0; i < NetworkTopology::nLeaves; i++) {
        std::string name = "leaf" + std::to_string(i + 1);
        uint32_t index = NetworkTopology::leafStartIdx + i;
        nodeNameToIndex[name] = index;
        nodeIndexToName[index] = name;
    }
    
    // Server节点: server1-server64
    for (uint32_t i = 0; i < NetworkTopology::nTotalServers; i++) {
        std::string name = "server" + std::to_string(i + 1);
        uint32_t index = NetworkTopology::serverStartIdx + i;
        nodeNameToIndex[name] = index;
        nodeIndexToName[index] = name;
    }
}

// 获取server所属的leaf索引
uint32_t getServerLeafIndex(uint32_t serverIndex) {
    uint32_t serverLocalIndex = serverIndex - NetworkTopology::serverStartIdx;
    return serverLocalIndex / NetworkTopology::nServersPerLeaf;
}

// 获取server在leaf内的本地索引
uint32_t getServerLocalIndex(uint32_t serverIndex) {
    uint32_t serverLocalIndex = serverIndex - NetworkTopology::serverStartIdx;
    return serverLocalIndex % NetworkTopology::nServersPerLeaf;
}

// 测量函数
static void Measurement(Ptr<ATPPacketSink> sink1, Ptr<ATPPacketSink> sink2) {
    Time now = Simulator::Now();
    
    uint64_t currentTimeJob1Bytes = sink1->GetTotalRxJob(1);
    uint64_t currentTimeJob2Bytes = sink2->GetTotalRxJob(2);
    
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;
    uint64_t ReceivedJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;
    
    SinkBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    SinkBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << ReceivedJob2BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
    lastTimeJob2Bytes = currentTimeJob2Bytes;
    
    Simulator::Schedule(MicroSeconds(100), &Measurement, sink1, sink2);
}

static void CwndChange_job1(uint32_t oldCwnd, uint32_t newCwnd) {
    cwndStream_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void CwndChange_job2(uint32_t oldCwnd, uint32_t newCwnd) {
    cwndStream_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

int main(int argc, char* argv[]) {
    // 命令行参数
    std::string jobsConfigFile = "contrib/atp/examples/spine8_leaf8_servers64_jobs2_seed42.json";
    std::string routeConfigFile = "contrib/atp/examples/spine8_leaf8_servers64_jobs2_seed42_aro.json";
    
    CommandLine cmd;
    cmd.AddValue("jobsConfig", "Jobs configuration file", jobsConfigFile);
    cmd.AddValue("routeConfig", "Route configuration file", routeConfigFile);
    cmd.Parse(argc, argv);
    
    // 打开跟踪文件
    cwndStream_job1.open("atp-result/trace-spineleaf-json/job1-cwnd-trace.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_job2.open("atp-result/trace-spineleaf-json/job2-cwnd-trace.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-spineleaf-json/job1-sinkBytes-trace.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-spineleaf-json/job2-sinkBytes-trace.txt", std::ofstream::out | std::ofstream::trunc);
    
    // 初始化节点映射
    initializeNodeMapping();
    
    // 解析配置文件
    std::vector<JobConfig> jobs = parseJobsConfig(jobsConfigFile);
    RouteConfig routeConfig = parseRouteConfig(routeConfigFile);
    
    // 仿真参数
    uint32_t maxBytes = 100;
    Time stopTime = Seconds(1.0) + Seconds(4.0);
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;
    uint64_t initialTimestamp = 1000000;
    
    // 写入初始拥塞窗口值
    cwndStream_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;
    
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(NetworkTopology::nTotalNodes);
    
    // 创建节点容器
    NodeContainer spineNodes, leafNodes, serverNodes;
    
    for (uint32_t i = 0; i < NetworkTopology::nSpines; i++) {
        spineNodes.Add(nodes.Get(NetworkTopology::spineStartIdx + i));
    }
    for (uint32_t i = 0; i < NetworkTopology::nLeaves; i++) {
        leafNodes.Add(nodes.Get(NetworkTopology::leafStartIdx + i));
    }
    for (uint32_t i = 0; i < NetworkTopology::nTotalServers; i++) {
        serverNodes.Add(nodes.Get(NetworkTopology::serverStartIdx + i));
    }
    
    NS_LOG_INFO("Create channels.");
    
    // 点对点链路配置
    PointToPointHelper p2pSpineLeaf;
    p2pSpineLeaf.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pSpineLeaf.SetChannelAttribute("Delay", StringValue("100us"));
    
    PointToPointHelper p2pLeafServer;
    p2pLeafServer.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pLeafServer.SetChannelAttribute("Delay", StringValue("100us"));
    
    // 存储网络设备和接口
    std::vector<std::vector<NetDeviceContainer>> spineLeafDevices(NetworkTopology::nSpines, 
                                                                  std::vector<NetDeviceContainer>(NetworkTopology::nLeaves));
    std::vector<std::vector<Ipv4InterfaceContainer>> spineLeafInterfaces(NetworkTopology::nSpines, 
                                                                         std::vector<Ipv4InterfaceContainer>(NetworkTopology::nLeaves));
    std::vector<std::vector<NetDeviceContainer>> leafServerDevices(NetworkTopology::nLeaves, 
                                                                   std::vector<NetDeviceContainer>(NetworkTopology::nServersPerLeaf));
    std::vector<std::vector<Ipv4InterfaceContainer>> leafServerInterfaces(NetworkTopology::nLeaves, 
                                                                          std::vector<Ipv4InterfaceContainer>(NetworkTopology::nServersPerLeaf));
    
    // 安装Internet协议栈
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);
    
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;
    
    // 1. 连接Spine和Leaf
    for (uint32_t spineIdx = 0; spineIdx < NetworkTopology::nSpines; spineIdx++) {
        for (uint32_t leafIdx = 0; leafIdx < NetworkTopology::nLeaves; leafIdx++) {
            NodeContainer linkNodes(spineNodes.Get(spineIdx), leafNodes.Get(leafIdx));
            NetDeviceContainer devices = p2pSpineLeaf.Install(linkNodes);
            spineLeafDevices[spineIdx][leafIdx] = devices;
            
            std::ostringstream subnet;
            subnet << "10." << spineIdx + 1 << "." << leafIdx + 1 << ".0";
            ipv4Helper.SetBase(subnet.str().c_str(), "255.255.255.0");
            Ipv4InterfaceContainer interfaces = ipv4Helper.Assign(devices);
            spineLeafInterfaces[spineIdx][leafIdx] = interfaces;
        }
    }
    
    // 2. 连接Leaf和Server
    for (uint32_t leafIdx = 0; leafIdx < NetworkTopology::nLeaves; leafIdx++) {
        for (uint32_t serverLocalIdx = 0; serverLocalIdx < NetworkTopology::nServersPerLeaf; serverLocalIdx++) {
            uint32_t globalServerIdx = leafIdx * NetworkTopology::nServersPerLeaf + serverLocalIdx;
            NodeContainer linkNodes(leafNodes.Get(leafIdx), serverNodes.Get(globalServerIdx));
            NetDeviceContainer devices = p2pLeafServer.Install(linkNodes);
            leafServerDevices[leafIdx][serverLocalIdx] = devices;
            
            std::ostringstream subnet;
            subnet << "192." << (leafIdx + 100) << "." << (serverLocalIdx + 1) * 10 << ".0";
            ipv4Helper.SetBase(subnet.str().c_str(), "255.255.255.0");
            Ipv4InterfaceContainer interfaces = ipv4Helper.Assign(devices);
            leafServerInterfaces[leafIdx][serverLocalIdx] = interfaces;
        }
    }
    
    // 设置队列阈值
    for (uint32_t spineIdx = 0; spineIdx < NetworkTopology::nSpines; spineIdx++) {
        for (uint32_t leafIdx = 0; leafIdx < NetworkTopology::nLeaves; leafIdx++) {
            Ptr<PointToPointNetDevice> spineDevice = 
                DynamicCast<PointToPointNetDevice>(spineLeafDevices[spineIdx][leafIdx].Get(0));
            if (spineDevice != 0) {
                spineDevice->SetThreshold(80);
                spineDevice->SetEnableEcn(false);
            }
        }
    }
    
    NS_LOG_INFO("Create Applications.");
    
    // 创建Parameter Server应用
    std::vector<Ptr<ATPPacketSink>> sinkApps;
    std::vector<Ptr<ATPSocket>> sinkSockets;
    std::vector<Address> sinkAddresses;
    
    for (const auto& job : jobs) {
        // 获取PS节点索引
        uint32_t psNodeIndex = nodeNameToIndex[job.ps];
        uint32_t psLeafIndex = getServerLeafIndex(psNodeIndex);
        uint32_t psLocalIndex = getServerLocalIndex(psNodeIndex);
        
        Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();
        
        uint16_t sinkPort = 9;
        Address sinkAddress(InetSocketAddress(leafServerInterfaces[psLeafIndex][psLocalIndex].GetAddress(1), sinkPort));
        sinkApp->SetAddressPort(sinkAddress, sinkPort);
        
        Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(psNodeIndex), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
        sinkApp->SetSocket(sinkATPSocket);
        sinkATPSocket->Bind(sinkAddress);
        sinkATPSocket->Listen();
        
        sinkApp->SetStartTime(Seconds(0.0));
        sinkApp->SetStopTime(stopTime);
        nodes.Get(psNodeIndex)->AddApplication(sinkApp);
        
        sinkApps.push_back(sinkApp);
        sinkSockets.push_back(sinkATPSocket);
        sinkAddresses.push_back(sinkAddress);
    }
    
    // 创建Worker应用
    std::vector<std::vector<Ptr<ATPBulkSendApplication>>> jobWorkerApps(jobs.size());
    std::vector<std::vector<Ptr<ATPSocket>>> jobWorkerSockets(jobs.size());
    
    for (size_t jobIdx = 0; jobIdx < jobs.size(); jobIdx++) {
        const auto& job = jobs[jobIdx];
        uint16_t sendPort = 11 + jobIdx;
        
        for (const auto& workerName : job.workers) {
            uint32_t workerNodeIndex = nodeNameToIndex[workerName];
            uint32_t workerLeafIndex = getServerLeafIndex(workerNodeIndex);
            uint32_t workerLocalIndex = getServerLocalIndex(workerNodeIndex);
            
            Ptr<ATPBulkSendApplication> workerApp = CreateObject<ATPBulkSendApplication>();
            Address workerAddress(InetSocketAddress(leafServerInterfaces[workerLeafIndex][workerLocalIndex].GetAddress(1), sendPort));
            
            Ptr<Socket> workerSocket = Socket::CreateSocket(nodes.Get(workerNodeIndex), ATPSocketFactory::GetTypeId());
            Ptr<ATPSocket> worker_ATPSocket = DynamicCast<ATPSocket>(workerSocket);
            
            // 设置初始拥塞窗口
            worker_ATPSocket->SetInitCwnd(jobIdx == 0 ? job1_initCwnd : job2_initCwnd);
            
            // 配置应用
            workerApp->Setup(sinkAddresses[jobIdx], worker_ATPSocket, maxBytes, job.id);
            workerApp->SetEnableATPTag(true);
            workerApp->SetStartTime(Seconds(1.0));
            workerApp->SetStopTime(stopTime);
            
            // 计算fanin degree和worker ID
            uint32_t faninDegree = 0;
            uint32_t workerId = 0;
            for (size_t i = 0; i < job.workers.size(); i++) {
                faninDegree |= (1 << i);
                if (job.workers[i] == workerName) {
                    workerId = (1 << i);
                }
            }
            workerApp->SetFaninDegree(faninDegree);
            workerApp->SetWorkerId(workerId);
            
            worker_ATPSocket->SetConnectCallback(
                MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, workerApp),
                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, workerApp));
            worker_ATPSocket->SetSendCallback(
                MakeCallback(&ATPBulkSendApplication::DataSend, workerApp));
            worker_ATPSocket->Bind(workerAddress);
            worker_ATPSocket->Connect(sinkAddresses[jobIdx]);
            
            nodes.Get(workerNodeIndex)->AddApplication(workerApp);
            
            jobWorkerApps[jobIdx].push_back(workerApp);
            jobWorkerSockets[jobIdx].push_back(worker_ATPSocket);
            
            // 连接拥塞窗口跟踪（每个job的第一个worker）
            if (jobWorkerApps[jobIdx].size() == 1) {
                if (jobIdx == 0) {
                    worker_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace", MakeCallback(&CwndChange_job1));
                } else {
                    worker_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace", MakeCallback(&CwndChange_job2));
                }
            }
        }
        
        // 添加地址映射到sink socket
        for (const auto& workerName : job.workers) {
            uint32_t workerNodeIndex = nodeNameToIndex[workerName];
            uint32_t workerLeafIndex = getServerLeafIndex(workerNodeIndex);
            uint32_t workerLocalIndex = getServerLocalIndex(workerNodeIndex);
            
            sinkSockets[jobIdx]->AddAddressMapping(
                job.id,
                leafServerInterfaces[workerLeafIndex][workerLocalIndex].GetAddress(1),
                sendPort);
        }
    }
    
    NS_LOG_INFO("Configure routing.");
    
    // 获取静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    std::vector<Ptr<ATPStaticRouting>> spineRouting(NetworkTopology::nSpines);
    std::vector<Ptr<ATPStaticRouting>> leafRouting(NetworkTopology::nLeaves);
    std::vector<Ptr<ATPStaticRouting>> serverRouting(NetworkTopology::nTotalServers);
    
    for (uint32_t i = 0; i < NetworkTopology::nSpines; i++) {
        spineRouting[i] = staticRoutingHelper.GetStaticRouting(spineNodes.Get(i)->GetObject<Ipv4>());
        spineRouting[i]->SetEnableAggregation(true);
    }
    for (uint32_t i = 0; i < NetworkTopology::nLeaves; i++) {
        leafRouting[i] = staticRoutingHelper.GetStaticRouting(leafNodes.Get(i)->GetObject<Ipv4>());
        leafRouting[i]->SetEnableAggregation(true);
    }
    for (uint32_t i = 0; i < NetworkTopology::nTotalServers; i++) {
        serverRouting[i] = staticRoutingHelper.GetStaticRouting(serverNodes.Get(i)->GetObject<Ipv4>());
    }
    
    // 配置聚合器的fanin degree
    for (uint32_t leafIdx = 0; leafIdx < NetworkTopology::nLeaves; leafIdx++) {
        Ptr<ATPL4Protocol> atpl4_leaf = leafRouting[leafIdx]->GetATPL4Protocol();
        
        // 为每个job配置leaf的聚合度
        for (const auto& job : jobs) {
            uint32_t leafWorkerMask = 0;
            for (const auto& workerName : job.workers) {
                uint32_t workerNodeIndex = nodeNameToIndex[workerName];
                uint32_t workerLeafIndex = getServerLeafIndex(workerNodeIndex);
                if (workerLeafIndex == leafIdx) {
                    // 计算worker在全局的位掩码
                    uint32_t globalWorkerIdx = workerNodeIndex - NetworkTopology::serverStartIdx;
                    leafWorkerMask |= (1 << globalWorkerIdx);
                }
            }
            if (leafWorkerMask != 0) {
                atpl4_leaf->SetAggregatorFaninDegree(job.id, leafWorkerMask);
            }
        }
    }
    
    for (uint32_t spineIdx = 0; spineIdx < NetworkTopology::nSpines; spineIdx++) {
        Ptr<ATPL4Protocol> atpl4_spine = spineRouting[spineIdx]->GetATPL4Protocol();
        
        // 为每个job配置spine的聚合度
        for (const auto& job : jobs) {
            uint32_t spineWorkerMask = 0;
            for (const auto& workerName : job.workers) {
                uint32_t workerNodeIndex = nodeNameToIndex[workerName];
                uint32_t globalWorkerIdx = workerNodeIndex - NetworkTopology::serverStartIdx;
                spineWorkerMask |= (1 << globalWorkerIdx);
            }
            atpl4_spine->SetAggregatorFaninDegree(job.id, spineWorkerMask);
        }
    }
    
    // 基于ARO配置的路由设置
    for (const auto& jobRouteEntry : routeConfig.jobRoutes) {
        uint32_t jobId = jobRouteEntry.first;
        const auto& routes = jobRouteEntry.second;
        for (const auto& routePair : routes) {
            const std::string& fromName = routePair.first;
            const std::string& toName = routePair.second;
            if (nodeNameToIndex.find(fromName) == nodeNameToIndex.end() || 
                nodeNameToIndex.find(toName) == nodeNameToIndex.end()) {
                continue; // 跳过无效的节点名称
            }
            
            uint32_t fromIndex = nodeNameToIndex[fromName];
            uint32_t toIndex = nodeNameToIndex[toName];
            
            // 获取目标节点的IP地址
            Ipv4Address toAddress;
            uint32_t interfaceId = 1; // 默认接口ID
            
            if (toIndex >= NetworkTopology::serverStartIdx) {
                // 目标是server
                uint32_t toLeafIndex = getServerLeafIndex(toIndex);
                uint32_t toLocalIndex = getServerLocalIndex(toIndex);
                toAddress = leafServerInterfaces[toLeafIndex][toLocalIndex].GetAddress(1);
            } else if (toIndex >= NetworkTopology::leafStartIdx) {
                // 目标是leaf - 需要通过spine-leaf接口访问
                uint32_t leafIdx = toIndex - NetworkTopology::leafStartIdx;
                // 使用第一个spine作为默认路由
                toAddress = spineLeafInterfaces[0][leafIdx].GetAddress(1);
            } else {
                // 目标是spine - 需要通过spine-leaf接口访问
                uint32_t spineIdx = toIndex - NetworkTopology::spineStartIdx;
                // 使用第一个leaf作为默认路由
                toAddress = spineLeafInterfaces[spineIdx][0].GetAddress(0);
            }
            
            // 获取下一跳地址
            Ipv4Address nextHop;
            
            if (fromIndex >= NetworkTopology::serverStartIdx) {
                // 从server出发
                uint32_t fromLeafIndex = getServerLeafIndex(fromIndex);
                uint32_t fromLocalIndex = getServerLocalIndex(fromIndex);
                nextHop = leafServerInterfaces[fromLeafIndex][fromLocalIndex].GetAddress(0); // 到leaf
                
                serverRouting[fromIndex - NetworkTopology::serverStartIdx]->AddHostRouteTo(toAddress, nextHop, interfaceId);
            } else if (fromIndex >= NetworkTopology::leafStartIdx) {
                // 从leaf出发
                uint32_t leafIdx = fromIndex - NetworkTopology::leafStartIdx;
                
                if (toIndex >= NetworkTopology::serverStartIdx) {
                    // leaf到server
                    uint32_t toLeafIndex = getServerLeafIndex(toIndex);
                    if (toLeafIndex == leafIdx) {
                        // 同一个leaf下的server，直接路由
                        uint32_t toLocalIndex = getServerLocalIndex(toIndex);
                        nextHop = leafServerInterfaces[leafIdx][toLocalIndex].GetAddress(1);
                        interfaceId = toLocalIndex + NetworkTopology::nSpines + 1; // leaf的server接口
                    } else {
                        // 不同leaf，通过spine路由
                        nextHop = spineLeafInterfaces[0][leafIdx].GetAddress(0); // 到spine
                        interfaceId = 1; // leaf的第一个spine接口
                    }
                } else {
                    // leaf到spine或其他leaf
                    nextHop = spineLeafInterfaces[0][leafIdx].GetAddress(0); // 到spine
                    interfaceId = 1;
                }
                
                leafRouting[leafIdx]->AddHostRouteTo(toAddress, nextHop, interfaceId);
            } else {
                // 从spine出发
                uint32_t spineIdx = fromIndex - NetworkTopology::spineStartIdx;
                
                if (toIndex >= NetworkTopology::serverStartIdx) {
                    // spine到server
                    uint32_t toLeafIndex = getServerLeafIndex(toIndex);
                    nextHop = spineLeafInterfaces[spineIdx][toLeafIndex].GetAddress(1); // 到leaf
                    interfaceId = toLeafIndex + 1;
                } else if (toIndex >= NetworkTopology::leafStartIdx) {
                    // spine到leaf
                    uint32_t leafIdx = toIndex - NetworkTopology::leafStartIdx;
                    nextHop = spineLeafInterfaces[spineIdx][leafIdx].GetAddress(1);
                    interfaceId = leafIdx + 1;
                }
                
                spineRouting[spineIdx]->AddHostRouteTo(toAddress, nextHop, interfaceId);
            }
        }
    }
    
    // 添加默认路由以确保连通性
    // Server到其他所有节点的默认路由
    for (uint32_t serverIdx = 0; serverIdx < NetworkTopology::nTotalServers; serverIdx++) {
        uint32_t leafIndex = getServerLeafIndex(NetworkTopology::serverStartIdx + serverIdx);
        uint32_t localIndex = getServerLocalIndex(NetworkTopology::serverStartIdx + serverIdx);
        Ipv4Address leafGateway = leafServerInterfaces[leafIndex][localIndex].GetAddress(0);
        
        // 到所有其他server的路由
        for (uint32_t targetServerIdx = 0; targetServerIdx < NetworkTopology::nTotalServers; targetServerIdx++) {
            if (targetServerIdx != serverIdx) {
                uint32_t targetLeafIndex = getServerLeafIndex(NetworkTopology::serverStartIdx + targetServerIdx);
                uint32_t targetLocalIndex = getServerLocalIndex(NetworkTopology::serverStartIdx + targetServerIdx);
                Ipv4Address targetAddress = leafServerInterfaces[targetLeafIndex][targetLocalIndex].GetAddress(1);
                
                serverRouting[serverIdx]->AddHostRouteTo(targetAddress, leafGateway, 1);
            }
        }
    }
    
    // Leaf的默认路由
    for (uint32_t leafIdx = 0; leafIdx < NetworkTopology::nLeaves; leafIdx++) {
        // 到其他leaf下server的路由，通过spine0
        Ipv4Address spineGateway = spineLeafInterfaces[0][leafIdx].GetAddress(0);
        
        for (uint32_t targetLeafIdx = 0; targetLeafIdx < NetworkTopology::nLeaves; targetLeafIdx++) {
            if (targetLeafIdx != leafIdx) {
                for (uint32_t serverLocalIdx = 0; serverLocalIdx < NetworkTopology::nServersPerLeaf; serverLocalIdx++) {
                    Ipv4Address targetAddress = leafServerInterfaces[targetLeafIdx][serverLocalIdx].GetAddress(1);
                    leafRouting[leafIdx]->AddHostRouteTo(targetAddress, spineGateway, 1);
                }
            }
        }
    }
    
    // Spine的路由
    for (uint32_t spineIdx = 0; spineIdx < NetworkTopology::nSpines; spineIdx++) {
        // 到所有server的路由
        for (uint32_t leafIdx = 0; leafIdx < NetworkTopology::nLeaves; leafIdx++) {
            Ipv4Address leafGateway = spineLeafInterfaces[spineIdx][leafIdx].GetAddress(1);
            
            for (uint32_t serverLocalIdx = 0; serverLocalIdx < NetworkTopology::nServersPerLeaf; serverLocalIdx++) {
                Ipv4Address serverAddress = leafServerInterfaces[leafIdx][serverLocalIdx].GetAddress(1);
                spineRouting[spineIdx]->AddHostRouteTo(serverAddress, leafGateway, leafIdx + 1);
            }
        }
    }
    
    // 开始测量
    if (sinkApps.size() >= 2) {
        Simulator::Schedule(Seconds(1.0), &Measurement, sinkApps[0], sinkApps[1]);
    }
    
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime + MicroSeconds(10));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
    
    // 关闭文件
    cwndStream_job1.close();
    cwndStream_job2.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    
    // 输出结果
    for (size_t i = 0; i < sinkApps.size(); i++) {
        std::cout << "Job " << (i+1) << " Total Bytes Received: " << sinkApps[i]->GetTotalRx() << std::endl;
    }
    
    return 0;
}
