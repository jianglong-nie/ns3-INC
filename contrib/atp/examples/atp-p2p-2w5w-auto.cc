/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// n workers for job1 and m workers for job2 to n5, n5 to n6
//
// - Flow from job1 workers and job2 workers to n6 using BulkSendApplication.

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
#include <sstream>
#include <vector>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-P2P-auto");

// 拥塞窗口跟踪，写入txt文件
ofstream cwndStream_n0_job1;
ofstream cwndStream_m0_job2;

// 添加队列长度跟踪文件流
ofstream queueSizeStream_n5;
uint32_t lastQueueSize = 0;  // 用于存储上一次记录的队列大小

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

ofstream SinkBytesStream_job1;
ofstream SinkBytesStream_job2;

// 记录接收端收到的总字节数
static void
Measurement(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = sink->GetTotalRxJob(1);  // job1
    uint64_t currentTimeJob2Bytes = sink->GetTotalRxJob(2);  // job2
        
    // 100us内接收到的字节数
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;
    uint64_t ReceivedJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;

    // 记录总字节数和本100us内接收的字节数
    SinkBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    SinkBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << ReceivedJob2BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
    lastTimeJob2Bytes = currentTimeJob2Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &Measurement, sink);
}

static void
CwndChange_n0_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n0_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_m0_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_m0_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

// 修改队列长度跟踪回调函数为定期采样
static void
SampleQueueSize(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_n5 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MicroSeconds(10), &SampleQueueSize, device);
}

int
main(int argc, char* argv[])
{
    // 在程序开始时打开文件流，使用trunc模式清空文件
    cwndStream_n0_job1.open("atp-result/trace-p2p-auto/n0-job1-cwnd-p2p-auto.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_m0_job2.open("atp-result/trace-p2p-auto/m0-job2-cwnd-p2p-auto.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-p2p-auto/job1-sinkBytes-p2p-auto.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-p2p-auto/job2-sinkBytes-p2p-auto.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_n5.open("atp-result/trace-p2p-auto/n5-queueSize-p2p-auto.txt", std::ofstream::out | std::ofstream::trunc);
    

    // 定义worker数量
    int n = 4;  // job1的worker数量
    int m = 8;  // job2的worker数量
    
    //LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);

    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(10000);

    // 设置job1和job2初始拥塞窗口
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;

    uint64_t initialTimestamp = 1000000;

    cwndStream_n0_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_m0_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;

    //
    // Explicitly create the nodes required by the topology
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(n + m + 2); // n个job1 workers + m个job2 workers + n5 + n6
    
    int n5_index = n + m;      // n5节点索引
    int n6_index = n + m + 1;  // n6节点索引

    NS_LOG_INFO("Create channels.");

    //
    // Create point-to-point links
    //

    // job1
    PointToPointHelper pointToPoint_job1;
    pointToPoint_job1.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint_job1.SetChannelAttribute("Delay", StringValue("2us"));

    // job2
    PointToPointHelper pointToPoint_job2;
    pointToPoint_job2.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint_job2.SetChannelAttribute("Delay", StringValue("10us"));

    // ps
    PointToPointHelper pointToPoint_ps;
    pointToPoint_ps.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint_ps.SetChannelAttribute("Delay", StringValue("2us"));

    // 存储所有worker到n5的连接
    std::vector<NetDeviceContainer> workerToN5Devices;
    
    // 创建job1 workers到n5的连接
    for (int i = 0; i < n; i++) {
        NodeContainer workerN5 = NodeContainer(nodes.Get(i), nodes.Get(n5_index));
        NetDeviceContainer devices = pointToPoint_job1.Install(workerN5);
        workerToN5Devices.push_back(devices);
    }
    
    // 创建job2 workers到n5的连接
    for (int i = 0; i < m; i++) {
        NodeContainer workerN5 = NodeContainer(nodes.Get(n + i), nodes.Get(n5_index));
        NetDeviceContainer devices = pointToPoint_job2.Install(workerN5);
        workerToN5Devices.push_back(devices);
    }

    // 创建n5到n6的连接
    NodeContainer n5n6 = NodeContainer(nodes.Get(n5_index), nodes.Get(n6_index));
    NetDeviceContainer n5n6Devices = pointToPoint_ps.Install(n5n6);

    // 设置n5上与n6连接部分的队列阈值
    Ptr<PointToPointNetDevice> n5Device = DynamicCast<PointToPointNetDevice>(n5n6Devices.Get(0));
    n5Device->SetEnableEcn(true);
    n5Device->SetThreshold(80);

    // 启动队列长度采样
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize, n5Device);

    //
    // Install the internet stack on the nodes
    //
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);

    //
    // Assign IP addresses
    //
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;
    
    // 存储所有worker到n5的接口
    std::vector<Ipv4InterfaceContainer> workerToN5Interfaces;
    
    // 为每个worker分配IP地址
    for (int i = 0; i < n + m; i++) {
        std::stringstream ss;
        ss << "10.1." << (i + 1) << ".0";
        ipv4Helper.SetBase(ss.str().c_str(), "255.255.255.0");
        workerToN5Interfaces.push_back(ipv4Helper.Assign(workerToN5Devices[i]));
    }

    // 为n5到n6分配IP地址
    ipv4Helper.SetBase("10.1.100.0", "255.255.255.0");
    Ipv4InterfaceContainer n5n6Interface = ipv4Helper.Assign(n5n6Devices);

    //
    // Create a PacketSinkApplication and install it on n6
    //
    NS_LOG_INFO("Create Applications.");

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();

    // add address and port
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(n5n6Interface.GetAddress(1), sinkPort));
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    // create ATPSocket and bind to sinkAddress
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(n6_index), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    // start sinkApp
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(n6_index)->AddApplication(sinkApp);

    //
    // Create sockets and applications for all workers
    //
    std::vector<Ptr<ATPSocket>> workerSockets;
    std::vector<Ptr<ATPBulkSendApplication>> workerApps;
    
    uint16_t sendPort = 11;   // Port for job1
    uint16_t sendPort2 = 12;  // Port for job2

    // 计算job1的fanin degree
    uint32_t job1FaninDegree = 0;
    for (int i = 0; i < n; i++) {
        job1FaninDegree |= (1 << i);
    }

    // 计算job2的fanin degree
    uint32_t job2FaninDegree = 0;
    for (int i = 0; i < m; i++) {
        job2FaninDegree |= (1 << i);
    }

    // 创建job1的workers
    for (int i = 0; i < n; i++) {
        // 创建socket
        Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(i), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> atpSocket = DynamicCast<ATPSocket>(socket);
        workerSockets.push_back(atpSocket);

        // 设置初始拥塞窗口
        atpSocket->SetInitCwnd(job1_initCwnd);

        // 创建应用程序
        Ptr<ATPBulkSendApplication> app = CreateObject<ATPBulkSendApplication>();
        workerApps.push_back(app);

        // 配置应用程序
        app->Setup(sinkAddress, atpSocket, maxBytes, 1);  // jobId = 1
        app->SetEnableATPTag(true);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(stopTime);
        app->SetFaninDegree(job1FaninDegree);
        app->SetWorkerId(1 << i);

        // 设置回调函数
        atpSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, app),
                                      MakeCallback(&ATPBulkSendApplication::ConnectionFailed, app));
        atpSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, app));

        // 绑定和连接
        Address workerAddress(InetSocketAddress(workerToN5Interfaces[i].GetAddress(0), sendPort));
        atpSocket->Bind(workerAddress);
        atpSocket->Connect(sinkAddress);

        // 添加到节点
        nodes.Get(i)->AddApplication(app);
        
        // 对job1的第一个worker添加拥塞窗口跟踪
        if (i == 0) {
            atpSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
                 MakeCallback(&CwndChange_n0_job1));
        }
    }

    // 创建job2的workers
    for (int i = 0; i < m; i++) {
        int nodeIndex = n + i;
        int interfaceIndex = n + i;

        // 创建socket
        Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(nodeIndex), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> atpSocket = DynamicCast<ATPSocket>(socket);
        workerSockets.push_back(atpSocket);

        // 设置初始拥塞窗口
        atpSocket->SetInitCwnd(job2_initCwnd);

        // 创建应用程序
        Ptr<ATPBulkSendApplication> app = CreateObject<ATPBulkSendApplication>();
        workerApps.push_back(app);

        // 配置应用程序
        app->Setup(sinkAddress, atpSocket, maxBytes, 2);  // jobId = 2
        app->SetEnableATPTag(true);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(stopTime);
        app->SetFaninDegree(job2FaninDegree);
        app->SetWorkerId(1 << i);

        // 设置回调函数
        atpSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, app),
                                      MakeCallback(&ATPBulkSendApplication::ConnectionFailed, app));
        atpSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, app));

        // 绑定和连接
        Address workerAddress(InetSocketAddress(workerToN5Interfaces[interfaceIndex].GetAddress(0), sendPort2));
        atpSocket->Bind(workerAddress);
        atpSocket->Connect(sinkAddress);

        // 添加到节点
        nodes.Get(nodeIndex)->AddApplication(app);
        
        // 对job2的第一个worker添加拥塞窗口跟踪
        if (i == 0) {
            atpSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
                 MakeCallback(&CwndChange_m0_job2));
        }
    }

    // 获取路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    std::vector<Ptr<ATPStaticRouting>> workerRoutings;
    
    // 获取所有worker的路由对象
    for (int i = 0; i < n + m; i++) {
        workerRoutings.push_back(staticRoutingHelper.GetStaticRouting(nodes.Get(i)->GetObject<Ipv4>()));
    }
    
    Ptr<ATPStaticRouting> staticRouting_n5 = staticRoutingHelper.GetStaticRouting(nodes.Get(n5_index)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n6 = staticRoutingHelper.GetStaticRouting(nodes.Get(n6_index)->GetObject<Ipv4>());

    staticRouting_n5->SetEnableAggregation(true);

    // 配置所有worker到n6的路由
    for (int i = 0; i < n + m; i++) {
        workerRoutings[i]->AddHostRouteTo(n5n6Interface.GetAddress(1), 
                                         workerToN5Interfaces[i].GetAddress(1), 1);
    }

    // 配置n5的路由表
    for (int i = 0; i < n + m; i++) {
        staticRouting_n5->AddHostRouteTo(workerToN5Interfaces[i].GetAddress(0), 
                                        workerToN5Interfaces[i].GetAddress(0), i + 1);
    }
    // n5到n6的路由
    staticRouting_n5->AddHostRouteTo(n5n6Interface.GetAddress(1), 
                                    n5n6Interface.GetAddress(1), n + m + 1);

    // 配置n6的路由表
    for (int i = 0; i < n + m; i++) {
        staticRouting_n6->AddHostRouteTo(workerToN5Interfaces[i].GetAddress(0), 
                                        n5n6Interface.GetAddress(0), 1);
    }

    // 添加地址映射
    // job1的地址映射
    for (int i = 0; i < n; i++) {
        sinkATPSocket->AddAddressMapping(1, workerToN5Interfaces[i].GetAddress(0), sendPort);
    }
    // job2的地址映射
    for (int i = 0; i < m; i++) {
        sinkATPSocket->AddAddressMapping(2, workerToN5Interfaces[n + i].GetAddress(0), sendPort2);
    }

    // 开始测量
    Simulator::Schedule(Seconds(1.0), &Measurement, sinkApp);

    //
    // Run simulation
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    // 关闭文件流
    cwndStream_n0_job1.close();
    cwndStream_m0_job2.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    queueSizeStream_n5.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
