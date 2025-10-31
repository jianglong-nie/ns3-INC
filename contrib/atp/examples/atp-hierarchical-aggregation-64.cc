/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// spine tree
// n0,n1(job1) and m0,m1(job2) to n2, n2 to n3
//
// - Flow from n0,n1(job1) and m0,m1(job2) to n3 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "tcp-bulk-send.tr"
//   and pcap tracing available when tracing is turned on

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/atp-module.h" // atp module
#include "ns3/ptr.h"
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-P2P-2w2w");

// 拥塞窗口跟踪，写入txt文件
std::ofstream cwndStream_job1;

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;

std::ofstream SinkBytesStream_job1;

// 记录接收端收到的总字节数
static void
Measurement(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = sink->GetTotalRxJob(1);  // job1
        
    // 100us内接收到的字节数
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;

    // 记录总字节数和本100us内接收的字节数
    SinkBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &Measurement, sink);
}

static void
CwndChange_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

int
main(int argc, char* argv[])
{
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);

    cwndStream_job1.open("atp-result/trace-ha-singlejob/job1-cwnd-trace-ha-singlejob.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-ha-singlejob/job1-sinkBytes-trace-ha-singlejob.txt", std::ofstream::out | std::ofstream::trunc);

    uint32_t maxBytes = 100;
    Time stopTime = Seconds(1.0) + Seconds(4.0); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    uint32_t nSpines = 8, nLeaves = 8, nWorkersPerLeaf = 8, nParameterServers = 1;
    uint32_t nWorkers = nWorkersPerLeaf * nLeaves;
    uint32_t nNodes = nSpines + nLeaves + nWorkers;
    nodes.Create(nNodes); // ps节点: w0

    // 节点索引分配
    uint32_t spineStartIdx = 0;
    uint32_t leafStartIdx = spineStartIdx + nSpines;
    uint32_t workerStartIdx = leafStartIdx + nLeaves;

    // 创建节点容器
    NodeContainer spineNodes;
    NodeContainer leafNodes;
    NodeContainer workerNodes;
    NodeContainer psNodes;

    for (uint32_t i = 0; i < nSpines; i++)
    {
        spineNodes.Add(nodes.Get(spineStartIdx + i));
    }
    for (uint32_t i = 0; i < nLeaves; i++)
    {
        leafNodes.Add(nodes.Get(leafStartIdx + i));
    }
    // 所有worker节点（包括将要作为PS的节点）
    for (uint32_t i = 0; i < nWorkersPerLeaf * nLeaves; i++)
    {
        workerNodes.Add(nodes.Get(workerStartIdx + i));
    }
    // 暂时从workers中抽取前nParameterServers个作为PS
    for (uint32_t i = 0; i < nParameterServers; i++)
    {
        psNodes.Add(workerNodes.Get(i));
    }


    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper p2pSpineLeaf;
    p2pSpineLeaf.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pSpineLeaf.SetChannelAttribute("Delay", StringValue("100us"));

    PointToPointHelper p2pLeafWorker;
    p2pLeafWorker.SetDeviceAttribute("DataRate", StringValue("200Gbps"));
    p2pLeafWorker.SetChannelAttribute("Delay", StringValue("100us"));

    // 存储所有网络设备和接口
    std::vector<std::vector<NetDeviceContainer>> spineLeafDevices(nSpines, std::vector<NetDeviceContainer>(nLeaves));
    std::vector<std::vector<Ipv4InterfaceContainer>> spineLeafInterfaces(nSpines, std::vector<Ipv4InterfaceContainer>(nLeaves));
    std::vector<std::vector<NetDeviceContainer>> leafWorkerDevices(nLeaves, std::vector<NetDeviceContainer>(nWorkersPerLeaf));
    std::vector<std::vector<Ipv4InterfaceContainer>> leafWorkerInterfaces(nLeaves, std::vector<Ipv4InterfaceContainer>(nWorkersPerLeaf));

    //
    // Install the internet stack on the nodes
    //
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);

    //
    // We've got the "hardware" in place.  Now we need to add IP addresses.
    //
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;

    // 1. 连接Spine和Leaf
    for (uint32_t spineIdx = 0; spineIdx < nSpines; spineIdx++)
    {
        for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
        {
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

    // 2. 连接Leaf和Worker（包括PS，因为PS也是worker）
    for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
    {
        for (uint32_t workerLocalIdx = 0; workerLocalIdx < nWorkersPerLeaf; workerLocalIdx++)
        {
            uint32_t globalWorkerIdx = leafIdx * nWorkersPerLeaf + workerLocalIdx;
            NodeContainer linkNodes(leafNodes.Get(leafIdx), workerNodes.Get(globalWorkerIdx));
            NetDeviceContainer devices = p2pLeafWorker.Install(linkNodes);
            leafWorkerDevices[leafIdx][workerLocalIdx] = devices;

            std::ostringstream subnet;
            subnet << "192." << (leafIdx + 100) << "." << (workerLocalIdx + 1) * 10 << ".0";
            ipv4Helper.SetBase(subnet.str().c_str(), "255.255.255.0");
            Ipv4InterfaceContainer interfaces = ipv4Helper.Assign(devices);
            leafWorkerInterfaces[leafIdx][workerLocalIdx] = interfaces;
        }
    }

    /*
    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p
    (大于100)

    // 直接在这个设备上设置自定义队列
    n2Device->SetQueue(customQueue);
    */

    // 设置队列阈值
    for (uint32_t spineIdx = 0; spineIdx < nSpines; spineIdx++)
    {
        for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
        {
            Ptr<PointToPointNetDevice> spineDevice =
                DynamicCast<PointToPointNetDevice>(spineLeafDevices[spineIdx][leafIdx].Get(0));
            if (spineDevice != nullptr)
            {
                spineDevice->SetThreshold(80);
                spineDevice->SetEnableEcn(false);
            }
        }
    }

    //
    // Create a PacketSinkApplication and install it on parameter servers
    //
    NS_LOG_INFO("Create Applications.");

    // 为每个parameter server创建sink应用
    std::vector<Ptr<ATPPacketSink>> sinkApps(nParameterServers);
    std::vector<Ptr<ATPSocket>> sinkSockets(nParameterServers);
    std::vector<Address> sinkAddresses(nParameterServers);

    for (uint32_t psIdx = 0; psIdx < nParameterServers; psIdx++)
    {
        sinkApps[psIdx] = CreateObject<ATPPacketSink>();

        // add address and port
        uint16_t sinkPort = 9;
        uint32_t psLeafIdx = psIdx / nWorkersPerLeaf;
        uint32_t psLocalWorkerIdx = psIdx % nWorkersPerLeaf;
        Address sinkAddress(
            InetSocketAddress(leafWorkerInterfaces[psLeafIdx][psLocalWorkerIdx].GetAddress(1), sinkPort));
        sinkApps[psIdx]->SetAddressPort(sinkAddress, sinkPort);

        // create ATPSocket and bind to sinkAddress
        Ptr<Socket> sinkSocket = Socket::CreateSocket(psNodes.Get(psIdx), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
        sinkApps[psIdx]->SetSocket(sinkATPSocket);
        sinkATPSocket->Bind(sinkAddress);
        sinkATPSocket->Listen();

        // start sinkApp
        sinkApps[psIdx]->SetStartTime(Seconds(0.0));
        sinkApps[psIdx]->SetStopTime(stopTime);
        psNodes.Get(psIdx)->AddApplication(sinkApps[psIdx]);

        sinkAddresses[psIdx] = sinkAddress;
        sinkSockets[psIdx] = sinkATPSocket;
    }
    //
    // Create sockets for job
    //

    uint16_t job1_sendPort = 11;  // Port for job1
    std::vector<Ptr<ATPSocket>> workerSockets(nWorkers, nullptr);
    std::vector<Ptr<ATPBulkSendApplication>> workerApps(nWorkers, nullptr);

    for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
    {
        for (uint32_t workerLocalIdx = 0; workerLocalIdx < nWorkersPerLeaf; workerLocalIdx++)
        {
            uint32_t workerIdx = leafIdx * nWorkersPerLeaf + workerLocalIdx;
            // 跳过作为PS的worker（它们已经运行sink应用）
            if (workerIdx < nParameterServers)
                continue;

            // Create and configure applications
            Ptr<ATPBulkSendApplication> workerApp = CreateObject<ATPBulkSendApplication>();
            Address workerAddress(
                InetSocketAddress(leafWorkerInterfaces[leafIdx][workerLocalIdx].GetAddress(1),
                                  job1_sendPort)); // 使用端口11

            Ptr<Socket> workerSocket =
                Socket::CreateSocket(workerNodes.Get(workerIdx), ATPSocketFactory::GetTypeId());
            Ptr<ATPSocket> worker_ATPSocket = DynamicCast<ATPSocket>(workerSocket);

            // 设置job1初始拥塞窗口
            worker_ATPSocket->SetInitCwnd(job1_initCwnd);

            // Configure w0job1App
            workerApp->Setup(sinkAddresses[0], worker_ATPSocket, maxBytes, 1);  //job1
            workerApp->SetEnableATPTag(true);
            workerApp->SetStartTime(Seconds(1.0));
            workerApp->SetStopTime(stopTime);
            if (leafIdx == 0)
            {
                workerApp->SetFaninDegree((1 << (nWorkersPerLeaf * nLeaves)) - 2); // 发送端
            }
            else
            {
                workerApp->SetFaninDegree((1 << (nWorkersPerLeaf * nLeaves)) - 1); // 发送端
            }    
            workerApp->SetWorkerId(1 << workerIdx);

            worker_ATPSocket->SetConnectCallback(
                MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, workerApp),
                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, workerApp));
            worker_ATPSocket->SetSendCallback(
                MakeCallback(&ATPBulkSendApplication::DataSend, workerApp));
            worker_ATPSocket->Bind(workerAddress);
            worker_ATPSocket->Connect(sinkAddresses[0]);

            workerNodes.Get(workerIdx)->AddApplication(workerApp);

            workerApps[workerIdx] = workerApp;
            workerSockets[workerIdx] = worker_ATPSocket;

            // 连接拥塞窗口跟踪
            if (workerIdx == nParameterServers) // 第一个普通worker
            {
                worker_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext(
                    "cwndTrace",
                    MakeCallback(&CwndChange_job1));
            }
        }
    }

    // 添加地址映射到sink sockets
    for (uint32_t psIdx = 0; psIdx < nParameterServers; psIdx++)
    {
        for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
        {
            for (uint32_t workerLocalIdx = 0; workerLocalIdx < nWorkersPerLeaf; workerLocalIdx++)
            {
                uint32_t workerIdx = leafIdx * nWorkersPerLeaf + workerLocalIdx;
                // 只映射普通worker，不包括PS
                if (workerIdx >= nParameterServers)
                {
                    sinkSockets[psIdx]->AddAddressMapping(
                        1, // job ID
                        leafWorkerInterfaces[leafIdx][workerLocalIdx].GetAddress(1),
                        job1_sendPort);
                }
            }
        }
    }


    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    std::vector<Ptr<ATPStaticRouting>> spineRouting(nSpines);
    std::vector<Ptr<ATPStaticRouting>> leafRouting(nLeaves);
    std::vector<Ptr<ATPStaticRouting>> workerRouting(nWorkers);
    std::vector<Ptr<ATPStaticRouting>> psRouting(nParameterServers);
    for (uint32_t i = 0; i < nSpines; i++)
    {
        spineRouting[i] =
            staticRoutingHelper.GetStaticRouting(spineNodes.Get(i)->GetObject<Ipv4>());
        spineRouting[i]->SetEnableAggregation(true);
    }
    for (uint32_t i = 0; i < nLeaves; i++)
    {
        leafRouting[i] = staticRoutingHelper.GetStaticRouting(leafNodes.Get(i)->GetObject<Ipv4>());
        leafRouting[i]->SetEnableAggregation(true);
    }
    for (uint32_t i = 0; i < nWorkers; i++)
    {
        workerRouting[i] =
            staticRoutingHelper.GetStaticRouting(workerNodes.Get(i)->GetObject<Ipv4>());
    }
    for (uint32_t i = 0; i < nParameterServers; i++)
    {
        psRouting[i] = staticRoutingHelper.GetStaticRouting(psNodes.Get(i)->GetObject<Ipv4>());
    }

    for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
    {
        Ptr<ATPL4Protocol> atpl4_leaf = leafRouting[leafIdx]->GetATPL4Protocol();
        uint32_t leafWorkerMask = 0;
        uint32_t workerLocalIdx = 0;
        if (leafIdx == 0)
            workerLocalIdx = 1;
        for (; workerLocalIdx < nWorkersPerLeaf; workerLocalIdx++)
        {
            leafWorkerMask |= (1 << (leafIdx * nWorkersPerLeaf + workerLocalIdx));
        }
        atpl4_leaf->SetAggregatorFaninDegree(1, leafWorkerMask); // 每个leaf聚合其下所有worker
    }

    for (uint32_t spineIdx = 0; spineIdx < nSpines; spineIdx++)
    {
        Ptr<ATPL4Protocol> atpl4_spine = spineRouting[spineIdx]->GetATPL4Protocol();
        uint32_t spineWorkerMask = (1 << (nWorkersPerLeaf * nLeaves)) - 2;
        atpl4_spine->SetAggregatorFaninDegree(1, spineWorkerMask); // 每个spine聚合其下所有leaf
    }

    // 所有Worker路由: worker -> leaf -> spine -> leaf -> PS
    for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
    {
        for (uint32_t workerLocalIdx = 0; workerLocalIdx < nWorkersPerLeaf; workerLocalIdx++)
        {
            uint32_t workerIdx = leafIdx * nWorkersPerLeaf + workerLocalIdx;

            // Worker到PS的路由：通过连接的leaf和spine
            // 连接0号PS
            if (workerIdx == 0)
                continue;
            Ipv4Address psAddress =
                leafWorkerInterfaces[0][0].GetAddress(1);

            workerRouting[workerIdx]->AddHostRouteTo(
                psAddress,                                              // PS地址
                leafWorkerInterfaces[leafIdx][workerLocalIdx].GetAddress(0), // 下一跳：leaf
                1);
        }
    }

    // PS路由: PS -> leaf -> spine -> leaf -> worker
    for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
    {
        for (uint32_t workerLocalIdx = 0; workerLocalIdx < nWorkersPerLeaf; workerLocalIdx++)
        {
            // PS到worker的路由：通过连接的leaf和spine
            // 连接0号PS
            Ipv4Address wAddress = leafWorkerInterfaces[leafIdx][workerLocalIdx].GetAddress(1);

            psRouting[0]->AddHostRouteTo(
                wAddress,                                 // PS地址
                leafWorkerInterfaces[0][0].GetAddress(0), // 下一跳：leaf
                1);
        }
    }

    // Leaf路由: 全部通过spine
    for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
    {
        for (uint32_t targetLeafIdx = 0; targetLeafIdx < nLeaves; targetLeafIdx++)
        {
            for (uint32_t workerIdx = 0; workerIdx < nWorkersPerLeaf; workerIdx++)
            {
                Ipv4Address targetWorkerAddress =
                    leafWorkerInterfaces[targetLeafIdx][workerIdx].GetAddress(1);
                leafRouting[leafIdx]->AddHostRouteTo(
                targetWorkerAddress,
                spineLeafInterfaces[0][leafIdx].GetAddress(0), // 下一跳：spine0
                2);
            }
        }
    }

    // Spine路由: 到所有leaf的直接路由
    for (uint32_t spineIdx = 0; spineIdx < nSpines; spineIdx++)
    {
        // Spine到leaf的路由
        for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
        {
            spineRouting[spineIdx]->AddHostRouteTo(
                spineLeafInterfaces[spineIdx][leafIdx].GetAddress(1),
                spineLeafInterfaces[spineIdx][leafIdx].GetAddress(1),
                leafIdx + 1);
        }

        // Spine到worker的路由：通过相应的leaf
        for (uint32_t leafIdx = 0; leafIdx < nLeaves; leafIdx++)
        {
            for (uint32_t workerIdx = 0; workerIdx < nWorkersPerLeaf; workerIdx++)
            {
                Ipv4Address workerAddress = leafWorkerInterfaces[leafIdx][workerIdx].GetAddress(1);
                spineRouting[spineIdx]->AddHostRouteTo(
                    workerAddress,
                    spineLeafInterfaces[spineIdx][leafIdx].GetAddress(1), // 下一跳：leaf
                    leafIdx + 1);
            }
        }
    }

    // 开始测量
    Simulator::Schedule(Seconds(1.0), &Measurement, sinkApps[0]);   //测量第一个任务

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime + MicroSeconds(10));
    
    /* 打印路由表用于调试
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    for (uint32_t i = 0; i < nNodes; i++)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<ATPStaticRouting> routing =
            staticRoutingHelper.GetStaticRouting(node->GetObject<Ipv4>());
        NS_LOG_INFO("Node " << i << " routing table:");
        routing->PrintRoutingTable(routingStream, Time::Unit::S); // 以秒为单位打印时间
    } */

    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    cwndStream_job1.close();

    std::cout << "Total Bytes Received: " << sinkApps[0]->GetTotalRx() << std::endl;

    return 0;
}
