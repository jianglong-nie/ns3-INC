/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology with dynamic job arrival and departure
// Three jobs: job1(w0,w1), job2(w2,w3), job3(w4,w5)
// Timeline:
// - t=1.0s: job1 and job2 start
// - t=1.0s+3000us: job3 joins
// - t=1.0s+6000us: job2 leaves
// - t=1.0s+9000us: job1 leaves
// - job3 continues until simulation ends

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

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-Dynamic-3Jobs");

// 拥塞窗口跟踪文件流
std::ofstream cwndStream_w0_job1;
std::ofstream cwndStream_w1_job1;
std::ofstream cwndStream_w2_job2;
std::ofstream cwndStream_w3_job2;
std::ofstream cwndStream_w4_job3;
std::ofstream cwndStream_w5_job3;

// 队列长度跟踪
std::ofstream queueSizeStream_s0;

// 每个job接收的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;
uint64_t lastTimeJob3Bytes = 0;

std::ofstream SinkBytesStream_job1;
std::ofstream SinkBytesStream_job2;
std::ofstream SinkBytesStream_job3;

// 记录接收端收到的总字节数
static void
Measurement(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = sink->GetTotalRxJob(1);
    uint64_t currentTimeJob2Bytes = sink->GetTotalRxJob(2);
    uint64_t currentTimeJob3Bytes = sink->GetTotalRxJob(3);
        
    // 100us内接收到的字节数
    uint64_t ReceivedJob1BytesPer100us = currentTimeJob1Bytes - lastTimeJob1Bytes;
    uint64_t ReceivedJob2BytesPer100us = currentTimeJob2Bytes - lastTimeJob2Bytes;
    uint64_t ReceivedJob3BytesPer100us = currentTimeJob3Bytes - lastTimeJob3Bytes;

    // 记录总字节数和本100us内接收的字节数
    SinkBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100us << std::endl;
    SinkBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << ReceivedJob2BytesPer100us << std::endl;
    SinkBytesStream_job3 << now.GetMicroSeconds() << "\t" << currentTimeJob3Bytes << "\t" << ReceivedJob3BytesPer100us << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
    lastTimeJob2Bytes = currentTimeJob2Bytes;
    lastTimeJob3Bytes = currentTimeJob3Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &Measurement, sink);
}

// 拥塞窗口变化回调函数
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

static void
CwndChange_w4_job3(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_w4_job3 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_w5_job3(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_w5_job3 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

// 队列长度定期采样
static void
SampleQueueSize(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_s0 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MicroSeconds(10), &SampleQueueSize, device);
}

int
main(int argc, char* argv[])
{
    // 打开文件流
    cwndStream_w0_job1.open("atp-result/trace-sigmetrics-dynamic-3jobs/w0-job1-cwnd.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w1_job1.open("atp-result/trace-sigmetrics-dynamic-3jobs/w1-job1-cwnd.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w2_job2.open("atp-result/trace-sigmetrics-dynamic-3jobs/w2-job2-cwnd.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w3_job2.open("atp-result/trace-sigmetrics-dynamic-3jobs/w3-job2-cwnd.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w4_job3.open("atp-result/trace-sigmetrics-dynamic-3jobs/w4-job3-cwnd.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w5_job3.open("atp-result/trace-sigmetrics-dynamic-3jobs/w5-job3-cwnd.txt", std::ofstream::out | std::ofstream::trunc);
    
    SinkBytesStream_job1.open("atp-result/trace-sigmetrics-dynamic-3jobs/job1-sinkBytes.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-sigmetrics-dynamic-3jobs/job2-sinkBytes.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job3.open("atp-result/trace-sigmetrics-dynamic-3jobs/job3-sinkBytes.txt", std::ofstream::out | std::ofstream::trunc);
    
    queueSizeStream_s0.open("atp-result/trace-sigmetrics-dynamic-3jobs/s0-queueSize.txt", std::ofstream::out | std::ofstream::trunc);

    // 仿真参数配置
    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(20000);

    // 动态时间线配置
    Time job1_startTime = Seconds(1.0);
    Time job1_stopTime = Seconds(1.0) + MicroSeconds(15000);
    
    Time job2_startTime = Seconds(1.0);
    Time job2_stopTime = Seconds(1.0) + MicroSeconds(12000);
    
    Time job3_startTime = Seconds(1.0) + MicroSeconds(3000);
    Time job3_stopTime = stopTime;

    // 初始拥塞窗口配置
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;
    uint32_t job3_initCwnd = 1;

    // 写入初始拥塞窗口值
    cwndStream_w0_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_w1_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_w2_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;
    cwndStream_w3_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;
    cwndStream_w4_job3 << (initialTimestamp + 3000) << "\t" << job3_initCwnd << std::endl;
    cwndStream_w5_job3 << (initialTimestamp + 3000) << "\t" << job3_initCwnd << std::endl;

    // 创建节点
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(9); // w0,w1,w2,w3,s0,ps,t0,w4,w5

    // 节点对容器
    NodeContainer w0s0 = NodeContainer(nodes.Get(0), nodes.Get(4)); // w0-s0
    NodeContainer w1s0 = NodeContainer(nodes.Get(1), nodes.Get(4)); // w1-s0
    NodeContainer w2s0 = NodeContainer(nodes.Get(2), nodes.Get(4)); // w2-s0
    NodeContainer w3s0 = NodeContainer(nodes.Get(3), nodes.Get(4)); // w3-s0
    NodeContainer s0ps = NodeContainer(nodes.Get(4), nodes.Get(5)); // s0-ps
    NodeContainer t0s0 = NodeContainer(nodes.Get(6), nodes.Get(4)); // t0-s0
    NodeContainer w4s0 = NodeContainer(nodes.Get(7), nodes.Get(4)); // w4-s0 (job3)
    NodeContainer w5s0 = NodeContainer(nodes.Get(8), nodes.Get(4)); // w5-s0 (job3)

    NS_LOG_INFO("Create channels.");

    // 创建点对点链路
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));

    NetDeviceContainer d0d2, d1d2, w2d2, w3d2, d2d3, t0d2, w4d2, w5d2;
    
    // 安装链路（所有链路延迟2us）
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    d0d2 = pointToPoint.Install(w0s0);
    d1d2 = pointToPoint.Install(w1s0);
    w2d2 = pointToPoint.Install(w2s0);
    w3d2 = pointToPoint.Install(w3s0);
    w4d2 = pointToPoint.Install(w4s0);
    w5d2 = pointToPoint.Install(w5s0);
    t0d2 = pointToPoint.Install(t0s0);
    d2d3 = pointToPoint.Install(s0ps);

    // 配置s0上的队列
    Ptr<PointToPointNetDevice> s0Device = DynamicCast<PointToPointNetDevice>(d2d3.Get(0));
    NS_ASSERT(s0Device != nullptr);
    s0Device->SetThreshold(300);
    s0Device->SetEnableEcn(true);
    
    // 启动队列长度采样
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize, s0Device);

    // 安装协议栈
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);

    // 分配IP地址
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;
    
    ipv4Helper.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4Helper.Assign(d0d2);

    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4Helper.Assign(d1d2);

    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iw2i2 = ipv4Helper.Assign(w2d2);

    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer iw3i2 = ipv4Helper.Assign(w3d2);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4Helper.Assign(d2d3);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer it0i2 = ipv4Helper.Assign(t0d2);

    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer iw4i2 = ipv4Helper.Assign(w4d2);

    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer iw5i2 = ipv4Helper.Assign(w5d2);

    //
    // 创建PacketSink应用
    //
    NS_LOG_INFO("Create Applications.");

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();

    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort));
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(5)->AddApplication(sinkApp);

    //
    // 为每个job创建sockets
    //
    Ptr<Socket> w0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w2job2socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w3job2socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w4job3socket = Socket::CreateSocket(nodes.Get(7), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w5job3socket = Socket::CreateSocket(nodes.Get(8), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w0job1_ATPSocket = DynamicCast<ATPSocket>(w0job1socket);
    Ptr<ATPSocket> w1job1_ATPSocket = DynamicCast<ATPSocket>(w1job1socket);
    Ptr<ATPSocket> w2job2_ATPSocket = DynamicCast<ATPSocket>(w2job2socket);
    Ptr<ATPSocket> w3job2_ATPSocket = DynamicCast<ATPSocket>(w3job2socket);
    Ptr<ATPSocket> w4job3_ATPSocket = DynamicCast<ATPSocket>(w4job3socket);
    Ptr<ATPSocket> w5job3_ATPSocket = DynamicCast<ATPSocket>(w5job3socket);

    // 创建应用
    Ptr<ATPBulkSendApplication> w0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w2job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w3job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w4job3App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w5job3App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort1 = 11;  // job1端口
    uint16_t sendPort2 = 12;  // job2端口
    uint16_t sendPort3 = 13;  // job3端口

    Address w0Address(InetSocketAddress(i0i2.GetAddress(0), sendPort1));
    Address w1Address(InetSocketAddress(i1i2.GetAddress(0), sendPort1));
    Address w2Address(InetSocketAddress(iw2i2.GetAddress(0), sendPort2));
    Address w3Address(InetSocketAddress(iw3i2.GetAddress(0), sendPort2));
    Address w4Address(InetSocketAddress(iw4i2.GetAddress(0), sendPort3));
    Address w5Address(InetSocketAddress(iw5i2.GetAddress(0), sendPort3));

    // ========== 配置 Job1 (w0, w1) ==========
    w0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w1job1_ATPSocket->SetInitCwnd(job1_initCwnd);

    w0job1App->Setup(sinkAddress, w0job1_ATPSocket, maxBytes, 1);
    w0job1App->SetEnableATPTag(true);
    w0job1App->SetStartTime(job1_startTime);
    w0job1App->SetStopTime(job1_stopTime);
    w0job1App->SetFaninDegree(0b00000011);
    w0job1App->SetWorkerId(0b00000001);

    w0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w0job1App));
    w0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w0job1App));
    w0job1_ATPSocket->Bind(w0Address);
    w0job1_ATPSocket->Connect(sinkAddress);
    nodes.Get(0)->AddApplication(w0job1App);

    w1job1App->Setup(sinkAddress, w1job1_ATPSocket, maxBytes, 1);
    w1job1App->SetEnableATPTag(true);
    w1job1App->SetStartTime(job1_startTime);
    w1job1App->SetStopTime(job1_stopTime);
    w1job1App->SetFaninDegree(0b00000011);
    w1job1App->SetWorkerId(0b00000010);

    w1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w1job1App));
    w1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w1job1App));
    w1job1_ATPSocket->Bind(w1Address);
    w1job1_ATPSocket->Connect(sinkAddress);
    nodes.Get(1)->AddApplication(w1job1App);

    // ========== 配置 Job2 (w2, w3) ==========
    w2job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w3job2_ATPSocket->SetInitCwnd(job2_initCwnd);

    w2job2App->Setup(sinkAddress, w2job2_ATPSocket, maxBytes, 2);
    w2job2App->SetEnableATPTag(true);
    w2job2App->SetStartTime(job2_startTime);
    w2job2App->SetStopTime(job2_stopTime);
    w2job2App->SetFaninDegree(0b00000011);
    w2job2App->SetWorkerId(0b00000001);

    w2job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w2job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w2job2App));
    w2job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w2job2App));
    w2job2_ATPSocket->Bind(w2Address);
    w2job2_ATPSocket->Connect(sinkAddress);
    nodes.Get(2)->AddApplication(w2job2App);

    w3job2App->Setup(sinkAddress, w3job2_ATPSocket, maxBytes, 2);
    w3job2App->SetEnableATPTag(true);
    w3job2App->SetStartTime(job2_startTime);
    w3job2App->SetStopTime(job2_stopTime);
    w3job2App->SetFaninDegree(0b00000011);
    w3job2App->SetWorkerId(0b00000010);

    w3job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w3job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w3job2App));
    w3job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w3job2App));
    w3job2_ATPSocket->Bind(w3Address);
    w3job2_ATPSocket->Connect(sinkAddress);
    nodes.Get(3)->AddApplication(w3job2App);

    // ========== 配置 Job3 (w4, w5) ==========
    w4job3_ATPSocket->SetInitCwnd(job3_initCwnd);
    w5job3_ATPSocket->SetInitCwnd(job3_initCwnd);

    w4job3App->Setup(sinkAddress, w4job3_ATPSocket, maxBytes, 3);
    w4job3App->SetEnableATPTag(true);
    w4job3App->SetStartTime(job3_startTime);
    w4job3App->SetStopTime(job3_stopTime);
    w4job3App->SetFaninDegree(0b00000011);
    w4job3App->SetWorkerId(0b00000001);

    w4job3_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w4job3App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w4job3App));
    w4job3_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w4job3App));
    w4job3_ATPSocket->Bind(w4Address);
    w4job3_ATPSocket->Connect(sinkAddress);
    nodes.Get(7)->AddApplication(w4job3App);

    w5job3App->Setup(sinkAddress, w5job3_ATPSocket, maxBytes, 3);
    w5job3App->SetEnableATPTag(true);
    w5job3App->SetStartTime(job3_startTime);
    w5job3App->SetStopTime(job3_stopTime);
    w5job3App->SetFaninDegree(0b00000011);
    w5job3App->SetWorkerId(0b00000010);

    w5job3_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w5job3App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w5job3App));
    w5job3_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w5job3App));
    w5job3_ATPSocket->Bind(w5Address);
    w5job3_ATPSocket->Connect(sinkAddress);
    nodes.Get(8)->AddApplication(w5job3App);

    //
    // TCP背景流量 (可选)
    //
    /*
    uint16_t bgPort = 8080;
    PacketSinkHelper bgSinkHelper("ns3::TcpSocketFactory", 
                                   InetSocketAddress(Ipv4Address::GetAny(), bgPort));
    ApplicationContainer bgSinkApp = bgSinkHelper.Install(nodes.Get(5));
    bgSinkApp.Start(Seconds(0.0));
    bgSinkApp.Stop(stopTime);

    Address bgSinkAddress = InetSocketAddress(i2i3.GetAddress(1), bgPort);
    BulkSendHelper bgSourceHelper("ns3::TcpSocketFactory", bgSinkAddress);
    bgSourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer bgSourceApp = bgSourceHelper.Install(nodes.Get(6));
    bgSourceApp.Start(Seconds(1.0) + MicroSeconds(4000));
    bgSourceApp.Stop(Seconds(1.0) + MicroSeconds(4500));
    */

    // 连接拥塞窗口跟踪
    w0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w0_job1));
    w1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w1_job1));
    w2job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w2_job2));
    w3job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w3_job2));
    w4job3_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w4_job3));
    w5job3_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_w5_job3));

    // 配置静态路由
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_w0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s0 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_t0 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w4 = staticRoutingHelper.GetStaticRouting(nodes.Get(7)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w5 = staticRoutingHelper.GetStaticRouting(nodes.Get(8)->GetObject<Ipv4>());

    // 配置s0为聚合器
    staticRouting_s0->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> atpl4_s0 = staticRouting_s0->GetATPL4Protocol();
    atpl4_s0->SetLayerId(1);
    atpl4_s0->SetAggregatorFaninDegree(1, 0b00000011);
    atpl4_s0->SetAggregatorFaninDegree(2, 0b00000011);
    atpl4_s0->SetAggregatorFaninDegree(3, 0b00000011);

    // 配置各节点路由表
    // w0 -> ps
    staticRouting_w0->AddHostRouteTo(i2i3.GetAddress(1), i0i2.GetAddress(1), 1);
    // w1 -> ps
    staticRouting_w1->AddHostRouteTo(i2i3.GetAddress(1), i1i2.GetAddress(1), 1);
    // w2 -> ps
    staticRouting_w2->AddHostRouteTo(i2i3.GetAddress(1), iw2i2.GetAddress(1), 1);
    // w3 -> ps
    staticRouting_w3->AddHostRouteTo(i2i3.GetAddress(1), iw3i2.GetAddress(1), 1);
    // w4 -> ps
    staticRouting_w4->AddHostRouteTo(i2i3.GetAddress(1), iw4i2.GetAddress(1), 1);
    // w5 -> ps
    staticRouting_w5->AddHostRouteTo(i2i3.GetAddress(1), iw5i2.GetAddress(1), 1);

    // s0路由表
    staticRouting_s0->AddHostRouteTo(i0i2.GetAddress(0), i0i2.GetAddress(0), 1);
    staticRouting_s0->AddHostRouteTo(i1i2.GetAddress(0), i1i2.GetAddress(0), 2);
    staticRouting_s0->AddHostRouteTo(iw2i2.GetAddress(0), iw2i2.GetAddress(0), 3);
    staticRouting_s0->AddHostRouteTo(iw3i2.GetAddress(0), iw3i2.GetAddress(0), 4);
    staticRouting_s0->AddHostRouteTo(i2i3.GetAddress(1), i2i3.GetAddress(1), 5);
    staticRouting_s0->AddHostRouteTo(it0i2.GetAddress(0), it0i2.GetAddress(0), 6);
    staticRouting_s0->AddHostRouteTo(iw4i2.GetAddress(0), iw4i2.GetAddress(0), 7);
    staticRouting_s0->AddHostRouteTo(iw5i2.GetAddress(0), iw5i2.GetAddress(0), 8);

    // ps路由表 (所有worker都通过s0转发)
    staticRouting_ps->AddHostRouteTo(i0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    staticRouting_ps->AddHostRouteTo(i1i2.GetAddress(0), i2i3.GetAddress(0), 1);
    staticRouting_ps->AddHostRouteTo(iw2i2.GetAddress(0), i2i3.GetAddress(0), 1);
    staticRouting_ps->AddHostRouteTo(iw3i2.GetAddress(0), i2i3.GetAddress(0), 1);
    staticRouting_ps->AddHostRouteTo(it0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    staticRouting_ps->AddHostRouteTo(iw4i2.GetAddress(0), i2i3.GetAddress(0), 1);
    staticRouting_ps->AddHostRouteTo(iw5i2.GetAddress(0), i2i3.GetAddress(0), 1);

    // t0 -> ps
    staticRouting_t0->AddHostRouteTo(i2i3.GetAddress(1), it0i2.GetAddress(1), 1);

    // 添加地址映射到sinkSocket
    sinkATPSocket->AddAddressMapping(1, i0i2.GetAddress(0), sendPort1);  // job1 - w0
    sinkATPSocket->AddAddressMapping(1, i1i2.GetAddress(0), sendPort1);  // job1 - w1
    sinkATPSocket->AddAddressMapping(2, iw2i2.GetAddress(0), sendPort2); // job2 - w2
    sinkATPSocket->AddAddressMapping(2, iw3i2.GetAddress(0), sendPort2); // job2 - w3
    sinkATPSocket->AddAddressMapping(3, iw4i2.GetAddress(0), sendPort3); // job3 - w4
    sinkATPSocket->AddAddressMapping(3, iw5i2.GetAddress(0), sendPort3); // job3 - w5

    // 开始测量
    Simulator::Schedule(Seconds(1.0), &Measurement, sinkApp);
    
    // 运行仿真
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    // 关闭文件流
    cwndStream_w0_job1.close();
    cwndStream_w1_job1.close();
    cwndStream_w2_job2.close();
    cwndStream_w3_job2.close();
    cwndStream_w4_job3.close();
    cwndStream_w5_job3.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    SinkBytesStream_job3.close();
    queueSizeStream_s0.close();

    // 输出统计信息
    std::cout << "\n========== Job Statistics ==========" << std::endl;
    std::cout << "\n--- Job1 (Start: " << job1_startTime.GetMicroSeconds() 
              << "us, Stop: " << job1_stopTime.GetMicroSeconds() << "us) ---" << std::endl;
    std::cout << "w0 total bytes sent: " << w0job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "w1 total bytes sent: " << w1job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job1-PS total recv bytes: " << sinkApp->GetTotalRxJob(1) << std::endl;
    
    std::cout << "\n--- Job2 (Start: " << job2_startTime.GetMicroSeconds() 
              << "us, Stop: " << job2_stopTime.GetMicroSeconds() << "us) ---" << std::endl;
    std::cout << "w2 total bytes sent: " << w2job2_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "w3 total bytes sent: " << w3job2_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job2-PS total recv bytes: " << sinkApp->GetTotalRxJob(2) << std::endl;

    std::cout << "\n--- Job3 (Start: " << job3_startTime.GetMicroSeconds() 
              << "us, Stop: " << job3_stopTime.GetMicroSeconds() << "us) ---" << std::endl;
    std::cout << "w4 total bytes sent: " << w4job3_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "w5 total bytes sent: " << w5job3_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job3-PS total recv bytes: " << sinkApp->GetTotalRxJob(3) << std::endl;

    std::cout << "\n--- Total ---" << std::endl;
    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    // 输出哈希冲突次数
    Ptr<ATPL4Protocol> s0_atpl4 = staticRouting_s0->GetATPL4Protocol();
    std::cout << "\n--- Hash Collision Counters ---" << std::endl;
    std::cout << "s0_atpl4 job1 hash collision: " << s0_atpl4->GetJobIdHashCollisionCounter(1) << std::endl;
    std::cout << "s0_atpl4 job2 hash collision: " << s0_atpl4->GetJobIdHashCollisionCounter(2) << std::endl;
    std::cout << "s0_atpl4 job3 hash collision: " << s0_atpl4->GetJobIdHashCollisionCounter(3) << std::endl;
    std::cout << "===================================\n" << std::endl;

    return 0;
}

