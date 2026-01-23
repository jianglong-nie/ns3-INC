/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// w0,w1(job1) and w2,w3(job2) to s0, s0 to ps
//
// - Flow from w0,w1(job1) and w2,w3(job2) to ps using BulkSendApplication.
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
std::ofstream cwndStream_w0_job1;
std::ofstream cwndStream_w1_job1;
std::ofstream cwndStream_w2_job2;
std::ofstream cwndStream_w3_job2;

// 添加队列长度跟踪文件流
std::ofstream queueSizeStream_s0;
uint32_t lastQueueSize = 0;  // 用于存储上一次记录的队列大小

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

std::ofstream SinkBytesStream_job1;
std::ofstream SinkBytesStream_job2;

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

// 修改队列长度跟踪回调函数为定期采样
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
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    
    // 在程序开始时打开文件流，使用trunc模式清空文件
    cwndStream_w0_job1.open("atp-result/trace-sigmetrics-dynamic/w0-job1-cwnd-dynamic.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w1_job1.open("atp-result/trace-sigmetrics-dynamic/w1-job1-cwnd-dynamic.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w2_job2.open("atp-result/trace-sigmetrics-dynamic/w2-job2-cwnd-dynamic.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_w3_job2.open("atp-result/trace-sigmetrics-dynamic/w3-job2-cwnd-dynamic.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-sigmetrics-dynamic/job1-sinkBytes-dynamic.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-sigmetrics-dynamic/job2-sinkBytes-dynamic.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_s0.open("atp-result/trace-sigmetrics-dynamic/s0-queueSize-dynamic.txt", std::ofstream::out | std::ofstream::trunc);
    

    uint32_t maxBytes = 100;
    Time stopTime = Seconds(1.0) + MicroSeconds(10000); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;

    // 设置超时重传参数
    // 当数据包发送后超过retxTimeout时间未收到ACK，将触发重传
    // retxCheckInterval是定期检查超时的间隔时间
    //Time retxTimeout = MicroSeconds(10);         // 重传超时时间：16us（可调整）
    //Time retxCheckInterval = MicroSeconds(1);   // 超时检查间隔：2us（可调整）

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
    nodes.Create(7); // 7个节点: w0,w1,w2,w3,s0,ps,t0

    NodeContainer w0s0 = NodeContainer(nodes.Get(0), nodes.Get(4)); // w0-s0
    NodeContainer w1s0 = NodeContainer(nodes.Get(1), nodes.Get(4)); // w1-s0
    NodeContainer w2s0 = NodeContainer(nodes.Get(2), nodes.Get(4)); // w2-s0
    NodeContainer w3s0 = NodeContainer(nodes.Get(3), nodes.Get(4)); // w3-s0
    NodeContainer s0ps = NodeContainer(nodes.Get(4), nodes.Get(5)); // s0-ps
    NodeContainer t0s0 = NodeContainer(nodes.Get(6), nodes.Get(4)); // t0-s0 (background traffic)

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));

    NetDeviceContainer d0d2, d1d2, w2d2, w3d2, d2d3, t0d2;
    
    // Install w0,w1 to s0 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    d0d2 = pointToPoint.Install(w0s0);
    d1d2 = pointToPoint.Install(w1s0);

    // Install w2,w3 to s0 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    w2d2 = pointToPoint.Install(w2s0);
    w3d2 = pointToPoint.Install(w3s0);

    // Install t0 to s0 link with 2us delay (background traffic)
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    t0d2 = pointToPoint.Install(t0s0);

    // Install s0 to ps link
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    d2d3 = pointToPoint.Install(s0ps);

    
    // 设置s0上与ps连接部分的队列阈值
    Ptr<PointToPointNetDevice> s0Device = DynamicCast<PointToPointNetDevice>(d2d3.Get(0));
    NS_ASSERT(s0Device != nullptr); // 确保转换成功
    /*
    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p (大于100)

    // 直接在这个设备上设置自定义队列
    s0Device->SetQueue(customQueue);
    */

    s0Device->SetThreshold(300);
    s0Device->SetEnableEcn(true);
    
    // 启动队列长度采样
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize, s0Device);

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

    //
    // Create a PacketSinkApplication and install it on node 3
    //
    NS_LOG_INFO("Create Applications.");

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();

    // add address and port
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort));
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    // create ATPSocket and bind to sinkAddress
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    // start sinkApp
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(5)->AddApplication(sinkApp);

    //
    // Create sockets for job1 and job2
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

    uint16_t sendPort = 11;  // Port for job1
    uint16_t sendPort2 = 12; // Different port for job2

    Address w0Address(InetSocketAddress(i0i2.GetAddress(0), sendPort));  // 使用端口11
    Address w1Address(InetSocketAddress(i1i2.GetAddress(0), sendPort));  // 使用端口11
    Address w2Address(InetSocketAddress(iw2i2.GetAddress(0), sendPort2));  // 使用端口12
    Address w3Address(InetSocketAddress(iw3i2.GetAddress(0), sendPort2));  // 使用端口12

    // 设置job1初始拥塞窗口
    w0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w1job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    //w0job1_ATPSocket->SetRetxTimeout(retxTimeout);
    //w0job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    //w1job1_ATPSocket->SetRetxTimeout(retxTimeout);
    //w1job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);

    // Configure w0job1App
    w0job1App->Setup(sinkAddress, w0job1_ATPSocket, maxBytes, 1);
    w0job1App->SetEnableATPTag(true);
    w0job1App->SetStartTime(Seconds(1.0));
    w0job1App->SetStopTime(stopTime);
    w0job1App->SetFaninDegree(0b00000011); // 2个发送端
    w0job1App->SetWorkerId(0b00000001);

    w0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w0job1App));
    w0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w0job1App));
    w0job1_ATPSocket->Bind(w0Address);
    w0job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(0)->AddApplication(w0job1App);

    // Configure w1job1App
    w1job1App->Setup(sinkAddress, w1job1_ATPSocket, maxBytes, 1);
    w1job1App->SetEnableATPTag(true);
    w1job1App->SetStartTime(Seconds(1.0));
    w1job1App->SetStopTime(stopTime);
    w1job1App->SetFaninDegree(0b00000011); // 2个发送端
    w1job1App->SetWorkerId(0b00000010);

    w1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w1job1App));
    w1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w1job1App));
    w1job1_ATPSocket->Bind(w1Address);
    w1job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(1)->AddApplication(w1job1App);

    // 设置job2初始拥塞窗口
    w2job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w3job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    //w2job2_ATPSocket->SetRetxTimeout(retxTimeout);
    //w2job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    //w3job2_ATPSocket->SetRetxTimeout(retxTimeout);
    //w3job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);

    // Configure w2job2App
    w2job2App->Setup(sinkAddress, w2job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    w2job2App->SetEnableATPTag(true);
    w2job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    w2job2App->SetStopTime(stopTime);
    w2job2App->SetFaninDegree(0b00000011); // 2个发送端
    w2job2App->SetWorkerId(0b00000001);

    w2job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w2job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w2job2App));
    w2job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w2job2App));
    w2job2_ATPSocket->Bind(w2Address);
    w2job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(2)->AddApplication(w2job2App);

    // Configure w3job2App
    w3job2App->Setup(sinkAddress, w3job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    w3job2App->SetEnableATPTag(true);
    w3job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    w3job2App->SetStopTime(stopTime);
    w3job2App->SetFaninDegree(0b00000011); // 2个发送端
    w3job2App->SetWorkerId(0b00000010);

    w3job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w3job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w3job2App));
    w3job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w3job2App));
    w3job2_ATPSocket->Bind(w3Address);
    w3job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(3)->AddApplication(w3job2App);

    
    //
    // Create TCP background traffic from t0 to ps
    //
    // TCP PacketSink on ps for background traffic
    uint16_t bgPort = 8080;
    PacketSinkHelper bgSinkHelper("ns3::TcpSocketFactory", 
                                   InetSocketAddress(Ipv4Address::GetAny(), bgPort));
    ApplicationContainer bgSinkApp = bgSinkHelper.Install(nodes.Get(5));
    bgSinkApp.Start(Seconds(0.0));
    bgSinkApp.Stop(stopTime);

    // TCP BulkSendApplication on t0 (intermittent background traffic)
    Address bgSinkAddress = InetSocketAddress(i2i3.GetAddress(1), bgPort);
    BulkSendHelper bgSourceHelper("ns3::TcpSocketFactory", bgSinkAddress);
    bgSourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer bgSourceApp = bgSourceHelper.Install(nodes.Get(6));
    
    // 时断时续的背景流量：启动和停止多次
    bgSourceApp.Start(Seconds(1.0) + MicroSeconds(4000));
    bgSourceApp.Stop(Seconds(1.0) + MicroSeconds(4500));
    
    // 第二次启动
    //ApplicationContainer bgSourceApp2 = bgSourceHelper.Install(nodes.Get(6));
    //bgSourceApp2.Start(Seconds(1.0) + MicroSeconds(15000));
    //bgSourceApp2.Stop(Seconds(1.0) + MicroSeconds(16000));
    
    // 第三次启动
    //ApplicationContainer bgSourceApp3 = bgSourceHelper.Install(nodes.Get(6));
    //bgSourceApp3.Start(Seconds(1.0) + MicroSeconds(22000));
    //bgSourceApp3.Stop(Seconds(1.0) + MicroSeconds(22800));
    

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
    Ptr<ATPStaticRouting> staticRouting_ps = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_t0 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());

    staticRouting_s0->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> atpl4_s0 = staticRouting_s0->GetATPL4Protocol();
    atpl4_s0->SetLayerId(1);
    atpl4_s0->SetAggregatorFaninDegree(1, 0b00000011);
    atpl4_s0->SetAggregatorFaninDegree(2, 0b00000011);

    // 配置w0的路由表
    // w0到ps的路由:通过s0转发
    staticRouting_w0->AddHostRouteTo(i2i3.GetAddress(1), i0i2.GetAddress(1), 1);

    // 配置w1的路由表  
    // w1到ps的路由:通过s0转发
    staticRouting_w1->AddHostRouteTo(i2i3.GetAddress(1), i1i2.GetAddress(1), 1);

    // 配置w2的路由表
    // w2到ps的路由:通过s0转发
    staticRouting_w2->AddHostRouteTo(i2i3.GetAddress(1), iw2i2.GetAddress(1), 1);

    // 配置w3的路由表
    // w3到ps的路由:通过s0转发
    staticRouting_w3->AddHostRouteTo(i2i3.GetAddress(1), iw3i2.GetAddress(1), 1);

    // 配置s0的路由表
    // s0到w0的路由
    staticRouting_s0->AddHostRouteTo(i0i2.GetAddress(0), i0i2.GetAddress(0), 1);
    // s0到w1的路由  
    staticRouting_s0->AddHostRouteTo(i1i2.GetAddress(0), i1i2.GetAddress(0), 2);
    // s0到w2的路由
    staticRouting_s0->AddHostRouteTo(iw2i2.GetAddress(0), iw2i2.GetAddress(0), 3);
    // s0到w3的路由
    staticRouting_s0->AddHostRouteTo(iw3i2.GetAddress(0), iw3i2.GetAddress(0), 4);
    // s0到ps的路由
    staticRouting_s0->AddHostRouteTo(i2i3.GetAddress(1), i2i3.GetAddress(1), 5);
    // s0到t0的路由
    staticRouting_s0->AddHostRouteTo(it0i2.GetAddress(0), it0i2.GetAddress(0), 6);

    // 配置ps的路由表
    // ps到w0的路由:通过s0转发
    staticRouting_ps->AddHostRouteTo(i0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // ps到w1的路由:通过s0转发
    staticRouting_ps->AddHostRouteTo(i1i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // ps到w2的路由:通过s0转发
    staticRouting_ps->AddHostRouteTo(iw2i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // ps到w3的路由:通过s0转发
    staticRouting_ps->AddHostRouteTo(iw3i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // ps到t0的路由:通过s0转发
    staticRouting_ps->AddHostRouteTo(it0i2.GetAddress(0), i2i3.GetAddress(0), 1);

    // 配置t0的路由表
    // t0到ps的路由:通过s0转发
    staticRouting_t0->AddHostRouteTo(i2i3.GetAddress(1), it0i2.GetAddress(1), 1);

    // 添加地址映射
    sinkATPSocket->AddAddressMapping(1, i0i2.GetAddress(0), sendPort);  // job1 - w0
    sinkATPSocket->AddAddressMapping(1, i1i2.GetAddress(0), sendPort);  // job1 - w1
    sinkATPSocket->AddAddressMapping(2, iw2i2.GetAddress(0), sendPort2); // job2 - w2
    sinkATPSocket->AddAddressMapping(2, iw3i2.GetAddress(0), sendPort2); // job2 - w3

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

    cwndStream_w0_job1.close();
    cwndStream_w1_job1.close();
    cwndStream_w2_job2.close();
    cwndStream_w3_job2.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    queueSizeStream_s0.close();

    std::cout << "-------------job1-------------------" << std::endl;
    std::cout << "w0 total bytes sent: " << w0job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "w1 total bytes sent: " << w1job1_ATPSocket->GetTotalTxBytes() << std::endl;
    
    std::cout << "-------------job2-------------------" << std::endl;
    std::cout << "w2 total bytes sent: " << w2job2_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "w3 total bytes sent: " << w3job2_ATPSocket->GetTotalTxBytes() << std::endl;

    std::cout << "--------------------------------" << std::endl;
    std::cout << "job1-PS total recv bytes: " << sinkApp->GetTotalRxJob(1) << std::endl;
    std::cout << "job2-PS total recv bytes: " << sinkApp->GetTotalRxJob(2) << std::endl;
    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    // Get atp l4 protocol
    Ptr<ATPL4Protocol> s0_atpl4 = staticRouting_s0->GetATPL4Protocol();

    // 输出哈希冲突次数
    std::cout << "s0_atpl4 job1 hash collision counter: " << s0_atpl4->GetJobIdHashCollisionCounter(1) << std::endl;
    std::cout << "s0_atpl4 job2 hash collision counter: " << s0_atpl4->GetJobIdHashCollisionCounter(2) << std::endl;

    return 0;
}
