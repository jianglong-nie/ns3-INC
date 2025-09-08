/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// n0,n1,n2,n3,n4(job1) to s1, s1 to s2, m0,m1(job2) to s2, s2 to d
//
// - Flow from n0,n1,n2,n3,n4(job1) to d via s1->s2, and m0,m1(job2) to d via s2
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

NS_LOG_COMPONENT_DEFINE("ATP-P2P-2w5w");

// 拥塞窗口跟踪，写入txt文件
ofstream cwndStream_n0_job1;
ofstream cwndStream_m0_job2;

// 添加队列长度跟踪文件流
ofstream queueSizeStream_s1;
ofstream queueSizeStream_s2;
uint32_t lastQueueSize_s1 = 0;  
uint32_t lastQueueSize_s2 = 0;  

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

// s1队列长度跟踪回调函数
static void
SampleQueueSize_s1(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_s1 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MicroSeconds(10), &SampleQueueSize_s1, device);
}

// s2队列长度跟踪回调函数
static void
SampleQueueSize_s2(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_s2 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MicroSeconds(10), &SampleQueueSize_s2, device);
}

int
main(int argc, char* argv[])
{
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    //LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    //LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    
    // 在程序开始时打开文件流，使用trunc模式清空文件
    cwndStream_n0_job1.open("atp-result/trace-p2p-water-filling/n0-job1-cwnd-p2p-water-filling.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_m0_job2.open("atp-result/trace-p2p-water-filling/m0-job2-cwnd-p2p-water-filling.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-p2p-water-filling/job1-sinkBytes-p2p-water-filling.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-p2p-water-filling/job2-sinkBytes-p2p-water-filling.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_s1.open("atp-result/trace-p2p-water-filling/s1-queueSize-p2p-water-filling.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_s2.open("atp-result/trace-p2p-water-filling/s2-queueSize-p2p-water-filling.txt", std::ofstream::out | std::ofstream::trunc);
    

    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(10000); // 9.6us为一个rtt时间，近似为10us，这里跑2000个rtt

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_n0_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_m0_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(10); // 10个节点: n0-n4,m0,m1,s1,s2,d (索引0-4,5,6,7,8,9)

    // 重新定义节点连接关系
    NodeContainer n0s1 = NodeContainer(nodes.Get(0), nodes.Get(7)); // n0-s1 (索引7是s1)
    NodeContainer n1s1 = NodeContainer(nodes.Get(1), nodes.Get(7)); // n1-s1
    NodeContainer n2s1 = NodeContainer(nodes.Get(2), nodes.Get(7)); // n2-s1
    NodeContainer n3s1 = NodeContainer(nodes.Get(3), nodes.Get(7)); // n3-s1
    NodeContainer n4s1 = NodeContainer(nodes.Get(4), nodes.Get(7)); // n4-s1
    NodeContainer m0s2 = NodeContainer(nodes.Get(5), nodes.Get(8)); // m0-s2 (索引5是m0，8是s2) 
    NodeContainer m1s2 = NodeContainer(nodes.Get(6), nodes.Get(8)); // m1-s2 (索引6是m1，8是s2)
    NodeContainer s1s2 = NodeContainer(nodes.Get(7), nodes.Get(8)); // s1-s2
    NodeContainer s2d = NodeContainer(nodes.Get(8), nodes.Get(9));  // s2-d (索引9是d)

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));

    NetDeviceContainer d0s1, d1s1, d2s1, d3s1, d4s1, m0s2Dev, m1s2Dev, s1s2Dev, s2dDev;
    
    // Install n0-n4 to s1 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2us"));
    d0s1 = pointToPoint.Install(n0s1);
    d1s1 = pointToPoint.Install(n1s1);
    d2s1 = pointToPoint.Install(n2s1);
    d3s1 = pointToPoint.Install(n3s1);
    d4s1 = pointToPoint.Install(n4s1);

    // Install m0-m1 to s2 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2us"));
    m0s2Dev = pointToPoint.Install(m0s2);
    m1s2Dev = pointToPoint.Install(m1s2);

    // Install s1 to s2 link
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2us"));
    s1s2Dev = pointToPoint.Install(s1s2);

    // Install s2 to d link
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2us"));
    s2dDev = pointToPoint.Install(s2d);

    // 设置s1和s2的队列阈值
    Ptr<PointToPointNetDevice> s1Device = DynamicCast<PointToPointNetDevice>(s1s2Dev.Get(0));
    Ptr<PointToPointNetDevice> s2Device = DynamicCast<PointToPointNetDevice>(s2dDev.Get(0));
    s1Device->SetThreshold(80);
    s2Device->SetThreshold(80);
    
    // 启动队列长度采样
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize_s1, s1Device);
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize_s2, s2Device);

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
    
    ipv4Helper.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0s1 = ipv4Helper.Assign(d0s1);

    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1s1 = ipv4Helper.Assign(d1s1);

    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2s1 = ipv4Helper.Assign(d2s1);

    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i3s1 = ipv4Helper.Assign(d3s1);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i4s1 = ipv4Helper.Assign(d4s1);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer im0s2 = ipv4Helper.Assign(m0s2Dev);

    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer im1s2 = ipv4Helper.Assign(m1s2Dev);

    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer is1s2 = ipv4Helper.Assign(s1s2Dev);

    ipv4Helper.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer is2d = ipv4Helper.Assign(s2dDev);

    //
    // Create a PacketSinkApplication and install it on node d (index 7)
    //
    NS_LOG_INFO("Create Applications.");

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();

    // add address and port
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(is2d.GetAddress(1), sinkPort));
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    // create ATPSocket and bind to sinkAddress
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(9), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    // start sinkApp
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(9)->AddApplication(sinkApp);

    //
    // Create sockets for job1 and job2 on n0, n1, n2, n3, n4, m0, m1
    //
    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n2job1socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n3job1socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n4job1socket = Socket::CreateSocket(nodes.Get(4), ATPSocketFactory::GetTypeId());
    Ptr<Socket> m0job2socket = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId()); // m0在索引5
    Ptr<Socket> m1job2socket = Socket::CreateSocket(nodes.Get(6), ATPSocketFactory::GetTypeId()); // m1在索引6

    Ptr<ATPSocket> n0job1_ATPSocket = DynamicCast<ATPSocket>(n0job1socket);
    Ptr<ATPSocket> n1job1_ATPSocket = DynamicCast<ATPSocket>(n1job1socket);
    Ptr<ATPSocket> n2job1_ATPSocket = DynamicCast<ATPSocket>(n2job1socket);
    Ptr<ATPSocket> n3job1_ATPSocket = DynamicCast<ATPSocket>(n3job1socket);
    Ptr<ATPSocket> n4job1_ATPSocket = DynamicCast<ATPSocket>(n4job1socket);
    Ptr<ATPSocket> m0job2_ATPSocket = DynamicCast<ATPSocket>(m0job2socket);
    Ptr<ATPSocket> m1job2_ATPSocket = DynamicCast<ATPSocket>(m1job2socket);

    // Create and configure applications for job1
    Ptr<ATPBulkSendApplication> n0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n2job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n3job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n4job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> m0job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> m1job2App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort = 11;  // Port for job1
    uint16_t sendPort2 = 12; // Different port for job2

    Address n0Address(InetSocketAddress(i0s1.GetAddress(0), sendPort));  
    Address n1Address(InetSocketAddress(i1s1.GetAddress(0), sendPort));  
    Address n2Address(InetSocketAddress(i2s1.GetAddress(0), sendPort));  
    Address n3Address(InetSocketAddress(i3s1.GetAddress(0), sendPort));  
    Address n4Address(InetSocketAddress(i4s1.GetAddress(0), sendPort));  
    Address m0Address(InetSocketAddress(im0s2.GetAddress(0), sendPort2));  
    Address m1Address(InetSocketAddress(im1s2.GetAddress(0), sendPort2));  

    // 设置job1初始拥塞窗口
    n0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    
    // Configure n0job1App
    n0job1App->Setup(sinkAddress, n0job1_ATPSocket, maxBytes, 1);
    n0job1App->SetEnableATPTag(true);
    n0job1App->SetStartTime(Seconds(1.0));
    n0job1App->SetStopTime(stopTime);
    n0job1App->SetFaninDegree(0b00011111);
    n0job1App->SetWorkerId(0b00000001);

    n0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n0job1App));
    n0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n0job1App));
    n0job1_ATPSocket->Bind(n0Address);
    n0job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(0)->AddApplication(n0job1App);

    // Configure n1job1App
    n1job1App->Setup(sinkAddress, n1job1_ATPSocket, maxBytes, 1);
    n1job1App->SetEnableATPTag(true);
    n1job1App->SetStartTime(Seconds(1.0));
    n1job1App->SetStopTime(stopTime);
    n1job1App->SetFaninDegree(0b00011111);
    n1job1App->SetWorkerId(0b00000010);

    n1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n1job1App));
    n1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n1job1App));
    n1job1_ATPSocket->Bind(n1Address);
    n1job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(1)->AddApplication(n1job1App);

    // Configure n2job1App
    n2job1App->Setup(sinkAddress, n2job1_ATPSocket, maxBytes, 1);
    n2job1App->SetEnableATPTag(true);
    n2job1App->SetStartTime(Seconds(1.0));
    n2job1App->SetStopTime(stopTime);
    n2job1App->SetFaninDegree(0b00011111);
    n2job1App->SetWorkerId(0b00000100);

    n2job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n2job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n2job1App));
    n2job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n2job1App));
    n2job1_ATPSocket->Bind(n2Address);
    n2job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(2)->AddApplication(n2job1App);

    // Configure n3job1App
    n3job1App->Setup(sinkAddress, n3job1_ATPSocket, maxBytes, 1);
    n3job1App->SetEnableATPTag(true);
    n3job1App->SetStartTime(Seconds(1.0));
    n3job1App->SetStopTime(stopTime);
    n3job1App->SetFaninDegree(0b00011111);
    n3job1App->SetWorkerId(0b00001000);

    n3job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n3job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n3job1App));
    n3job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n3job1App));
    n3job1_ATPSocket->Bind(n3Address);
    n3job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(3)->AddApplication(n3job1App);

    // Configure n4job1App
    n4job1App->Setup(sinkAddress, n4job1_ATPSocket, maxBytes, 1);
    n4job1App->SetEnableATPTag(true);
    n4job1App->SetStartTime(Seconds(1.0));
    n4job1App->SetStopTime(stopTime);
    n4job1App->SetFaninDegree(0b00011111);
    n4job1App->SetWorkerId(0b00010000);

    n4job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n4job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n4job1App));
    n4job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n4job1App));
    n4job1_ATPSocket->Bind(n4Address);
    n4job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(4)->AddApplication(n4job1App);

    // 设置job2初始拥塞窗口
    m0job2_ATPSocket->SetInitCwnd(job2_initCwnd);

    // Configure m0job2App
    m0job2App->Setup(sinkAddress, m0job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    m0job2App->SetEnableATPTag(true);
    m0job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    m0job2App->SetStopTime(stopTime);
    m0job2App->SetFaninDegree(0b00000011);
    m0job2App->SetWorkerId(0b00000001);

    m0job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, m0job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, m0job2App));
    m0job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, m0job2App));
    m0job2_ATPSocket->Bind(m0Address);
    m0job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(5)->AddApplication(m0job2App); // m0在索引5

    // Configure m1job2App
    m1job2App->Setup(sinkAddress, m1job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    m1job2App->SetEnableATPTag(true);
    m1job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    m1job2App->SetStopTime(stopTime);
    m1job2App->SetFaninDegree(0b00000011);
    m1job2App->SetWorkerId(0b00000010);

    m1job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, m1job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, m1job2App));
    m1job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, m1job2App));
    m1job2_ATPSocket->Bind(m1Address);
    m1job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(6)->AddApplication(m1job2App); // m1在索引6

    // 连接拥塞窗口跟踪
    n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job1));
    m0job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_m0_job2));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_n0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n4 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_m0 = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>()); // m0在索引5
    Ptr<ATPStaticRouting> staticRouting_m1 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>()); // m1在索引6
    Ptr<ATPStaticRouting> staticRouting_s1 = staticRoutingHelper.GetStaticRouting(nodes.Get(7)->GetObject<Ipv4>()); // s1在索引7
    Ptr<ATPStaticRouting> staticRouting_s2 = staticRoutingHelper.GetStaticRouting(nodes.Get(8)->GetObject<Ipv4>()); // s2在索引8
    Ptr<ATPStaticRouting> staticRouting_d = staticRoutingHelper.GetStaticRouting(nodes.Get(9)->GetObject<Ipv4>());  // d在索引9

    // 在s1和s2开启聚合
    staticRouting_s1->SetEnableAggregation(true);
    staticRouting_s2->SetEnableAggregation(true);

    // 配置n0的路由表 - n0到d的路由:通过s1转发
    staticRouting_n0->AddHostRouteTo(is2d.GetAddress(1), i0s1.GetAddress(1), 1);

    // 配置n1的路由表 - n1到d的路由:通过s1转发  
    staticRouting_n1->AddHostRouteTo(is2d.GetAddress(1), i1s1.GetAddress(1), 1);

    // 配置n2的路由表 - n2到d的路由:通过s1转发
    staticRouting_n2->AddHostRouteTo(is2d.GetAddress(1), i2s1.GetAddress(1), 1);

    // 配置n3的路由表 - n3到d的路由:通过s1转发
    staticRouting_n3->AddHostRouteTo(is2d.GetAddress(1), i3s1.GetAddress(1), 1);

    // 配置n4的路由表 - n4到d的路由:通过s1转发
    staticRouting_n4->AddHostRouteTo(is2d.GetAddress(1), i4s1.GetAddress(1), 1);

    // 配置m0的路由表 - m0到d的路由:通过s2转发
    staticRouting_m0->AddHostRouteTo(is2d.GetAddress(1), im0s2.GetAddress(1), 1);

    // 配置m1的路由表 - m1到d的路由:通过s2转发
    staticRouting_m1->AddHostRouteTo(is2d.GetAddress(1), im1s2.GetAddress(1), 1);

    // 配置s1的路由表
    // s1到d的路由:通过s2转发 (接口6是s1连接s2的接口)
    staticRouting_s1->AddHostRouteTo(is2d.GetAddress(1), is1s2.GetAddress(1), 6);

    // 配置s2的路由表
    // s2到n0-n4的路由：通过s1转发 (接口3是s2连接s1的接口)
    staticRouting_s2->AddHostRouteTo(i0s1.GetAddress(0), is1s2.GetAddress(0), 3);
    staticRouting_s2->AddHostRouteTo(i1s1.GetAddress(0), is1s2.GetAddress(0), 3);
    staticRouting_s2->AddHostRouteTo(i2s1.GetAddress(0), is1s2.GetAddress(0), 3);
    staticRouting_s2->AddHostRouteTo(i3s1.GetAddress(0), is1s2.GetAddress(0), 3);
    staticRouting_s2->AddHostRouteTo(i4s1.GetAddress(0), is1s2.GetAddress(0), 3);
    // s2到d的路由 (接口4是s2连接d的接口)
    staticRouting_s2->AddHostRouteTo(is2d.GetAddress(1), Ipv4Address::GetZero(), 4);

    // 配置d的路由表
    // d到n0-n4的路由:通过s2转发 (d只有一个接口连接s2)
    staticRouting_d->AddHostRouteTo(i0s1.GetAddress(0), is2d.GetAddress(0), 1);
    staticRouting_d->AddHostRouteTo(i1s1.GetAddress(0), is2d.GetAddress(0), 1);
    staticRouting_d->AddHostRouteTo(i2s1.GetAddress(0), is2d.GetAddress(0), 1);
    staticRouting_d->AddHostRouteTo(i3s1.GetAddress(0), is2d.GetAddress(0), 1);
    staticRouting_d->AddHostRouteTo(i4s1.GetAddress(0), is2d.GetAddress(0), 1);
    // d到m0-m1的路由:通过s2转发
    staticRouting_d->AddHostRouteTo(im0s2.GetAddress(0), is2d.GetAddress(0), 1);
    staticRouting_d->AddHostRouteTo(im1s2.GetAddress(0), is2d.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket->AddAddressMapping(1, i0s1.GetAddress(0), sendPort);  // job1 - n0
    sinkATPSocket->AddAddressMapping(1, i1s1.GetAddress(0), sendPort);  // job1 - n1
    sinkATPSocket->AddAddressMapping(1, i2s1.GetAddress(0), sendPort);  // job1 - n2
    sinkATPSocket->AddAddressMapping(1, i3s1.GetAddress(0), sendPort);  // job1 - n3
    sinkATPSocket->AddAddressMapping(1, i4s1.GetAddress(0), sendPort);  // job1 - n4
    sinkATPSocket->AddAddressMapping(2, im0s2.GetAddress(0), sendPort2); // job2 - m0
    sinkATPSocket->AddAddressMapping(2, im1s2.GetAddress(0), sendPort2); // job2 - m1

    // 开始测量
    Simulator::Schedule(Seconds(1.0), &Measurement, sinkApp);
    
    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    cwndStream_n0_job1.close();
    cwndStream_m0_job2.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    queueSizeStream_s1.close();
    queueSizeStream_s2.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
