/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// w0,w1(job1) to s0, s0 to ps1
// w2,w3(job2) to s0, s0 to ps2
//
// - Flow from w0,w1(job1) to ps1 and w2,w3(job2) to ps2 using BulkSendApplication.
// - Tracing of queues and packet receptions to file
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

NS_LOG_COMPONENT_DEFINE("ATP-Dispatch-2Jobs");

// 拥塞窗口跟踪，写入txt文件
std::ofstream cwndStream_w0_job1;
std::ofstream cwndStream_w1_job1;
std::ofstream cwndStream_w2_job2;
std::ofstream cwndStream_w3_job2;

// 添加队列长度跟踪文件流
std::ofstream queueSizeStream_s0_ps1;
std::ofstream queueSizeStream_s0_ps2;

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

std::ofstream SinkBytesStream_job1;
std::ofstream SinkBytesStream_job2;

// 记录job1接收端收到的总字节数
static void
MeasurementJob1(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = sink->GetTotalRxJob(1);  // job1
        
    // 100us内接收到的字节数
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;

    // 记录总字节数和本100us内接收的字节数
    SinkBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &MeasurementJob1, sink);
}

// 记录job2接收端收到的总字节数
static void
MeasurementJob2(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob2Bytes = sink->GetTotalRxJob(2);  // job2
        
    // 100us内接收到的字节数
    uint64_t ReceivedJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;

    // 记录总字节数和本100us内接收的字节数
    SinkBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << ReceivedJob2BytesPer100ms << std::endl;
    
    lastTimeJob2Bytes = currentTimeJob2Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &MeasurementJob2, sink);
}

static void
CwndChange_w0_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_w0_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_w1_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_w1_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_w2_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_w2_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_w3_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_w3_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

// 定期采样s0->ps1队列长度
static void
SampleQueueSize_s0_ps1(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_s0_ps1 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MicroSeconds(10), &SampleQueueSize_s0_ps1, device);
}

// 定期采样s0->ps2队列长度
static void
SampleQueueSize_s0_ps2(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_s0_ps2 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MicroSeconds(10), &SampleQueueSize_s0_ps2, device);
}

int
main(int argc, char* argv[])
{
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    
    // 在程序开始时打开文件流，使用trunc模式清空文件
    cwndStream_w0_job1.open("atp-result/trace-sigmetrics-dispatch/w0-job1-cwnd-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w1_job1.open("atp-result/trace-sigmetrics-dispatch/w1-job1-cwnd-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w2_job2.open("atp-result/trace-sigmetrics-dispatch/w2-job2-cwnd-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w3_job2.open("atp-result/trace-sigmetrics-dispatch/w3-job2-cwnd-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-sigmetrics-dispatch/job1-sinkBytes-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-sigmetrics-dispatch/job2-sinkBytes-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_s0_ps1.open("atp-result/trace-sigmetrics-dispatch/s0-ps1-queueSize-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_s0_ps2.open("atp-result/trace-sigmetrics-dispatch/s0-ps2-queueSize-dispatch.txt", std::ofstream::out | std::ofstream::trunc);
    

    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(10000); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_w0_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_w1_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_w2_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;
    cwndStream_w3_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(7); // 7个节点: w0,w1,w2,w3,s0,ps1,ps2

    NodeContainer w0s0 = NodeContainer(nodes.Get(0), nodes.Get(4)); // w0-s0
    NodeContainer w1s0 = NodeContainer(nodes.Get(1), nodes.Get(4)); // w1-s0
    NodeContainer w2s0 = NodeContainer(nodes.Get(2), nodes.Get(4)); // w2-s0
    NodeContainer w3s0 = NodeContainer(nodes.Get(3), nodes.Get(4)); // w3-s0
    NodeContainer s0ps1 = NodeContainer(nodes.Get(4), nodes.Get(5)); // s0-ps1
    NodeContainer s0ps2 = NodeContainer(nodes.Get(4), nodes.Get(6)); // s0-ps2

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));

    NetDeviceContainer dw0s0, dw1s0, dw2s0, dw3s0, ds0ps1, ds0ps2;
    
    // Install w0,w1,w2,w3 to s0 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    dw0s0 = pointToPoint.Install(w0s0);
    dw1s0 = pointToPoint.Install(w1s0);
    dw2s0 = pointToPoint.Install(w2s0);
    dw3s0 = pointToPoint.Install(w3s0);

    // Install s0 to ps1, ps2 links
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    ds0ps1 = pointToPoint.Install(s0ps1);
    ds0ps2 = pointToPoint.Install(s0ps2);

    
    // 设置s0上与ps1,ps2连接部分的队列阈值
    Ptr<PointToPointNetDevice> s0ps1Device = DynamicCast<PointToPointNetDevice>(ds0ps1.Get(0));
    Ptr<PointToPointNetDevice> s0ps2Device = DynamicCast<PointToPointNetDevice>(ds0ps2.Get(0));
    NS_ASSERT(s0ps1Device != nullptr); // 确保转换成功
    NS_ASSERT(s0ps2Device != nullptr); // 确保转换成功

    s0ps1Device->SetThreshold(300);
    s0ps1Device->SetEnableEcn(true);
    
    s0ps2Device->SetThreshold(300);
    s0ps2Device->SetEnableEcn(true);
    
    // 启动队列长度采样
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize_s0_ps1, s0ps1Device);
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize_s0_ps2, s0ps2Device);

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
    Ipv4InterfaceContainer iw0s0 = ipv4Helper.Assign(dw0s0);

    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iw1s0 = ipv4Helper.Assign(dw1s0);

    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iw2s0 = ipv4Helper.Assign(dw2s0);

    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer iw3s0 = ipv4Helper.Assign(dw3s0);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer is0ps1 = ipv4Helper.Assign(ds0ps1);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer is0ps2 = ipv4Helper.Assign(ds0ps2);

    //
    // Create PacketSinkApplications: ps1 for job1, ps2 for job2
    //
    NS_LOG_INFO("Create Applications.");

    // Sink for job1 on ps1
    Ptr<ATPPacketSink> sinkApp1 = CreateObject<ATPPacketSink>();
    uint16_t sinkPort1 = 9;
    Address sinkAddress1(InetSocketAddress(is0ps1.GetAddress(1), sinkPort1));
    sinkApp1->SetAddressPort(sinkAddress1, sinkPort1);

    Ptr<Socket> sinkSocket1 = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket1 = DynamicCast<ATPSocket>(sinkSocket1);
    sinkApp1->SetSocket(sinkATPSocket1);
    sinkATPSocket1->Bind(sinkAddress1);
    sinkATPSocket1->Listen();

    sinkApp1->SetStartTime(Seconds(0.0));
    sinkApp1->SetStopTime(stopTime);
    nodes.Get(5)->AddApplication(sinkApp1);

    // Sink for job2 on ps2
    Ptr<ATPPacketSink> sinkApp2 = CreateObject<ATPPacketSink>();
    uint16_t sinkPort2 = 9;
    Address sinkAddress2(InetSocketAddress(is0ps2.GetAddress(1), sinkPort2));
    sinkApp2->SetAddressPort(sinkAddress2, sinkPort2);

    Ptr<Socket> sinkSocket2 = Socket::CreateSocket(nodes.Get(6), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket2 = DynamicCast<ATPSocket>(sinkSocket2);
    sinkApp2->SetSocket(sinkATPSocket2);
    sinkATPSocket2->Bind(sinkAddress2);
    sinkATPSocket2->Listen();

    sinkApp2->SetStartTime(Seconds(0.0));
    sinkApp2->SetStopTime(stopTime);
    nodes.Get(6)->AddApplication(sinkApp2);

    //
    // Create sockets for job1 (w0, w1) and job2 (w2, w3)
    //
    Ptr<Socket> w0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w2job2socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w3job2socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w0job1_ATPSocket = DynamicCast<ATPSocket>(w0job1socket);
    Ptr<ATPSocket> w1job1_ATPSocket = DynamicCast<ATPSocket>(w1job1socket);
    Ptr<ATPSocket> w2job2_ATPSocket = DynamicCast<ATPSocket>(w2job2socket);
    Ptr<ATPSocket> w3job2_ATPSocket = DynamicCast<ATPSocket>(w3job2socket);

    // Create and configure applications
    Ptr<ATPBulkSendApplication> w0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w2job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w3job2App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort1 = 11;  // Port for job1
    uint16_t sendPort2 = 12;  // Port for job2

    Address w0Address(InetSocketAddress(iw0s0.GetAddress(0), sendPort1));
    Address w1Address(InetSocketAddress(iw1s0.GetAddress(0), sendPort1));
    Address w2Address(InetSocketAddress(iw2s0.GetAddress(0), sendPort2));
    Address w3Address(InetSocketAddress(iw3s0.GetAddress(0), sendPort2));

    // 设置job1初始拥塞窗口
    w0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w1job1_ATPSocket->SetInitCwnd(job1_initCwnd);

    // Configure w0job1App
    w0job1App->Setup(sinkAddress1, w0job1_ATPSocket, maxBytes, 1);
    w0job1App->SetEnableATPTag(true);
    w0job1App->SetStartTime(Seconds(1.0));
    w0job1App->SetStopTime(stopTime);
    w0job1App->SetFaninDegree(0b00000011); // 2个发送端
    w0job1App->SetWorkerId(0b00000001);

    w0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w0job1App));
    w0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w0job1App));
    w0job1_ATPSocket->Bind(w0Address);
    w0job1_ATPSocket->Connect(sinkAddress1);

    nodes.Get(0)->AddApplication(w0job1App);

    // Configure w1job1App
    w1job1App->Setup(sinkAddress1, w1job1_ATPSocket, maxBytes, 1);
    w1job1App->SetEnableATPTag(true);
    w1job1App->SetStartTime(Seconds(1.0));
    w1job1App->SetStopTime(stopTime);
    w1job1App->SetFaninDegree(0b00000011); // 2个发送端
    w1job1App->SetWorkerId(0b00000010);

    w1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w1job1App));
    w1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w1job1App));
    w1job1_ATPSocket->Bind(w1Address);
    w1job1_ATPSocket->Connect(sinkAddress1);

    nodes.Get(1)->AddApplication(w1job1App);

    // 设置job2初始拥塞窗口
    w2job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w3job2_ATPSocket->SetInitCwnd(job2_initCwnd);

    // Configure w2job2App
    w2job2App->Setup(sinkAddress2, w2job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    w2job2App->SetEnableATPTag(true);
    w2job2App->SetStartTime(Seconds(1.0));
    w2job2App->SetStopTime(stopTime);
    w2job2App->SetFaninDegree(0b00000011); // 2个发送端
    w2job2App->SetWorkerId(0b00000001);

    w2job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w2job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w2job2App));
    w2job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w2job2App));
    w2job2_ATPSocket->Bind(w2Address);
    w2job2_ATPSocket->Connect(sinkAddress2);

    nodes.Get(2)->AddApplication(w2job2App);

    // Configure w3job2App
    w3job2App->Setup(sinkAddress2, w3job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    w3job2App->SetEnableATPTag(true);
    w3job2App->SetStartTime(Seconds(1.0));
    w3job2App->SetStopTime(stopTime);
    w3job2App->SetFaninDegree(0b00000011); // 2个发送端
    w3job2App->SetWorkerId(0b00000010);

    w3job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w3job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w3job2App));
    w3job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w3job2App));
    w3job2_ATPSocket->Bind(w3Address);
    w3job2_ATPSocket->Connect(sinkAddress2);

    nodes.Get(3)->AddApplication(w3job2App);

    // 连接拥塞窗口跟踪
    w0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w0_job1));
    w1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w1_job1));
    w2job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w2_job2));
    w3job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w3_job2));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_w0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s0 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps1 = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps2 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());

    staticRouting_s0->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> atpl4_s0 = staticRouting_s0->GetATPL4Protocol();
    atpl4_s0->SetLayerId(1);
    atpl4_s0->SetAggregatorFaninDegree(1, 0b00000011); // job1: 2 workers
    atpl4_s0->SetAggregatorFaninDegree(2, 0b00000011); // job2: 2 workers

    // 配置w0的路由表
    // w0到ps1的路由:通过s0转发
    staticRouting_w0->AddHostRouteTo(is0ps1.GetAddress(1), iw0s0.GetAddress(1), 1);

    // 配置w1的路由表  
    // w1到ps1的路由:通过s0转发
    staticRouting_w1->AddHostRouteTo(is0ps1.GetAddress(1), iw1s0.GetAddress(1), 1);

    // 配置w2的路由表
    // w2到ps2的路由:通过s0转发
    staticRouting_w2->AddHostRouteTo(is0ps2.GetAddress(1), iw2s0.GetAddress(1), 1);

    // 配置w3的路由表
    // w3到ps2的路由:通过s0转发
    staticRouting_w3->AddHostRouteTo(is0ps2.GetAddress(1), iw3s0.GetAddress(1), 1);

    // 配置s0的路由表
    // s0到w0的路由
    staticRouting_s0->AddHostRouteTo(iw0s0.GetAddress(0), iw0s0.GetAddress(0), 1);
    // s0到w1的路由  
    staticRouting_s0->AddHostRouteTo(iw1s0.GetAddress(0), iw1s0.GetAddress(0), 2);
    // s0到w2的路由
    staticRouting_s0->AddHostRouteTo(iw2s0.GetAddress(0), iw2s0.GetAddress(0), 3);
    // s0到w3的路由
    staticRouting_s0->AddHostRouteTo(iw3s0.GetAddress(0), iw3s0.GetAddress(0), 4);
    // s0到ps1的路由
    staticRouting_s0->AddHostRouteTo(is0ps1.GetAddress(1), is0ps1.GetAddress(1), 5);
    // s0到ps2的路由
    staticRouting_s0->AddHostRouteTo(is0ps2.GetAddress(1), is0ps2.GetAddress(1), 6);

    // 配置ps1的路由表
    // ps1到w0的路由:通过s0转发
    staticRouting_ps1->AddHostRouteTo(iw0s0.GetAddress(0), is0ps1.GetAddress(0), 1);
    // ps1到w1的路由:通过s0转发
    staticRouting_ps1->AddHostRouteTo(iw1s0.GetAddress(0), is0ps1.GetAddress(0), 1);

    // 配置ps2的路由表
    // ps2到w2的路由:通过s0转发
    staticRouting_ps2->AddHostRouteTo(iw2s0.GetAddress(0), is0ps2.GetAddress(0), 1);
    // ps2到w3的路由:通过s0转发
    staticRouting_ps2->AddHostRouteTo(iw3s0.GetAddress(0), is0ps2.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket1->AddAddressMapping(1, iw0s0.GetAddress(0), sendPort1);  // job1 - w0
    sinkATPSocket1->AddAddressMapping(1, iw1s0.GetAddress(0), sendPort1);  // job1 - w1
    
    sinkATPSocket2->AddAddressMapping(2, iw2s0.GetAddress(0), sendPort2);  // job2 - w2
    sinkATPSocket2->AddAddressMapping(2, iw3s0.GetAddress(0), sendPort2);  // job2 - w3

    // 开始测量
    Simulator::Schedule(Seconds(1.0), &MeasurementJob1, sinkApp1);
    Simulator::Schedule(Seconds(1.0), &MeasurementJob2, sinkApp2);
    
    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    cwndStream_w0_job1.close();
    cwndStream_w1_job1.close();
    cwndStream_w2_job2.close();
    cwndStream_w3_job2.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    queueSizeStream_s0_ps1.close();
    queueSizeStream_s0_ps2.close();

    std::cout << "-------------job1-------------------" << std::endl;
    std::cout << "w0 total bytes sent: " << w0job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "w1 total bytes sent: " << w1job1_ATPSocket->GetTotalTxBytes() << std::endl;
    
    std::cout << "-------------job2-------------------" << std::endl;
    std::cout << "w2 total bytes sent: " << w2job2_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "w3 total bytes sent: " << w3job2_ATPSocket->GetTotalTxBytes() << std::endl;

    std::cout << "--------------------------------" << std::endl;
    std::cout << "job1-PS1 total recv bytes: " << sinkApp1->GetTotalRxJob(1) << std::endl;
    std::cout << "job2-PS2 total recv bytes: " << sinkApp2->GetTotalRxJob(2) << std::endl;

    // Get atp l4 protocol
    Ptr<ATPL4Protocol> s0_atpl4 = staticRouting_s0->GetATPL4Protocol();

    // 输出哈希冲突次数
    std::cout << "s0_atpl4 job1 hash collision counter: " << s0_atpl4->GetJobIdHashCollisionCounter(1) << std::endl;
    std::cout << "s0_atpl4 job2 hash collision counter: " << s0_atpl4->GetJobIdHashCollisionCounter(2) << std::endl;

    return 0;
}

