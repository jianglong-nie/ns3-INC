/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// n0,n1,n2,n3,n4(job1) and m0,m1(job2) to n5, n5 to n6
//
// - Flow from n0,n1,n2,n3,n4(job1) and m0,m1(job2) to n6 using BulkSendApplication.
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
#include "ns3/queue.h"
#include "ns3/queue-size.h" 
#include "ns3/drop-tail-queue.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-P2P-2w5w");

// 拥塞窗口跟踪，写入txt文件
ofstream cwndStream_n0_job1;
ofstream cwndStream_n1_job1;
ofstream cwndStream_n2_job1;
ofstream cwndStream_n3_job1;
ofstream cwndStream_n4_job1;
ofstream cwndStream_m0_job2;
ofstream cwndStream_m1_job2;

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
        
    // 100ms内接收到的字节数
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;
    uint64_t ReceivedJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;

    // 记录总字节数和本100ms内接收的字节数
    SinkBytesStream_job1 << now.GetSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    SinkBytesStream_job2 << now.GetSeconds() << "\t" << currentTimeJob2Bytes << "\t" << ReceivedJob2BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
    lastTimeJob2Bytes = currentTimeJob2Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MilliSeconds(100), &Measurement, sink);
}

static void
CwndChange_n0_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n0_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n1_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n1_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n2_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n2_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n3_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n3_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n4_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n4_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_m0_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_m0_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_m1_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_m1_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

// 修改队列长度跟踪回调函数为定期采样
static void
SampleQueueSize(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_n5 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MilliSeconds(1), &SampleQueueSize, device);
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
    cwndStream_n0_job1.open("atp-result/trace-p2p-2w5w/n0-job1-cwnd-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n1_job1.open("atp-result/trace-p2p-2w5w/n1-job1-cwnd-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n2_job1.open("atp-result/trace-p2p-2w5w/n2-job1-cwnd-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n3_job1.open("atp-result/trace-p2p-2w5w/n3-job1-cwnd-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n4_job1.open("atp-result/trace-p2p-2w5w/n4-job1-cwnd-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_m0_job2.open("atp-result/trace-p2p-2w5w/m0-job2-cwnd-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_m1_job2.open("atp-result/trace-p2p-2w5w/m1-job2-cwnd-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-p2p-2w5w/job1-sinkBytes-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-p2p-2w5w/job2-sinkBytes-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_n5.open("atp-result/trace-p2p-2w5w/n5-queueSize-p2p-2w5w.txt", std::ofstream::out | std::ofstream::trunc);
    

    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(2000); // 9.6us为一个rtt时间，近似为10us，这里跑2000个rtt

    uint64_t initialTimestamp = 1000000;
    // 设置job1和job2初始拥塞窗口
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_n0_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_n1_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_n2_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_n3_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_n4_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_m0_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;
    cwndStream_m1_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(9); // 增加到9个节点: n0-n4,m0,m1,n5,n6

    NodeContainer n0n5 = NodeContainer(nodes.Get(0), nodes.Get(7)); // n0-n5
    NodeContainer n1n5 = NodeContainer(nodes.Get(1), nodes.Get(7)); // n1-n5
    NodeContainer n2n5 = NodeContainer(nodes.Get(2), nodes.Get(7)); // n2-n5
    NodeContainer n3n5 = NodeContainer(nodes.Get(3), nodes.Get(7)); // n3-n5
    NodeContainer n4n5 = NodeContainer(nodes.Get(4), nodes.Get(7)); // n4-n5
    NodeContainer m0n5 = NodeContainer(nodes.Get(5), nodes.Get(7)); // m0-n5
    NodeContainer m1n5 = NodeContainer(nodes.Get(6), nodes.Get(7)); // m1-n5
    NodeContainer n5n6 = NodeContainer(nodes.Get(7), nodes.Get(8)); // n5-n6

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer d0d5, d1d5, d2d5, d3d5, d4d5, m0d5, m1d5, d5d6;
    d0d5 = pointToPoint.Install(n0n5);
    d1d5 = pointToPoint.Install(n1n5);
    d2d5 = pointToPoint.Install(n2n5);
    d3d5 = pointToPoint.Install(n3n5);
    d4d5 = pointToPoint.Install(n4n5);
    m0d5 = pointToPoint.Install(m0n5);
    m1d5 = pointToPoint.Install(m1n5);
    d5d6 = pointToPoint.Install(n5n6);

    // 获取 n5->n6 链路上的 n5 端的设备
    Ptr<PointToPointNetDevice> n5Device = DynamicCast<PointToPointNetDevice>(d5d6.Get(0));
    NS_ASSERT(n5Device != nullptr); // 确保转换成功

    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p (大于100)

    // 直接在这个设备上设置自定义队列
    n5Device->SetQueue(customQueue);

    // 设置n5上与n6连接部分的 ECN 阈值
    n5Device->SetThreshold(200); // 现在可以安全地设置为 100

    // 启动队列长度采样
    Simulator::Schedule(Seconds(0.9), &SampleQueueSize, n5Device);

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
    Ipv4InterfaceContainer i0i5 = ipv4Helper.Assign(d0d5);

    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i5 = ipv4Helper.Assign(d1d5);

    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i5 = ipv4Helper.Assign(d2d5);

    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i5 = ipv4Helper.Assign(d3d5);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i4i5 = ipv4Helper.Assign(d4d5);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer im0i5 = ipv4Helper.Assign(m0d5);

    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer im1i5 = ipv4Helper.Assign(m1d5);

    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer i5i6 = ipv4Helper.Assign(d5d6);

    //
    // Create a PacketSinkApplication and install it on node 6
    //
    NS_LOG_INFO("Create Applications.");

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();

    // add address and port
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(i5i6.GetAddress(1), sinkPort));
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    // create ATPSocket and bind to sinkAddress
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(8), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    // start sinkApp
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(8)->AddApplication(sinkApp);

    //
    // Create sockets for job1 and job2 on n0, n1, n2, n3, n4
    //
    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n2job1socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n3job1socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n4job1socket = Socket::CreateSocket(nodes.Get(4), ATPSocketFactory::GetTypeId());
    Ptr<Socket> m0job2socket = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId());
    Ptr<Socket> m1job2socket = Socket::CreateSocket(nodes.Get(6), ATPSocketFactory::GetTypeId());

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

    Address n0Address(InetSocketAddress(i0i5.GetAddress(0), sendPort));  // 使用端口11
    Address n1Address(InetSocketAddress(i1i5.GetAddress(0), sendPort));  // 使用端口11
    Address n2Address(InetSocketAddress(i2i5.GetAddress(0), sendPort));  // 使用端口11
    Address n3Address(InetSocketAddress(i3i5.GetAddress(0), sendPort));  // 使用端口11
    Address n4Address(InetSocketAddress(i4i5.GetAddress(0), sendPort));  // 使用端口11
    Address m0Address(InetSocketAddress(im0i5.GetAddress(0), sendPort2));  // 使用端口12
    Address m1Address(InetSocketAddress(im1i5.GetAddress(0), sendPort2));  // 使用端口12

    // 设置job1初始拥塞窗口
    n0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    n1job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    n2job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    n3job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    n4job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    
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
    m1job2_ATPSocket->SetInitCwnd(job2_initCwnd);

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

    nodes.Get(5)->AddApplication(m0job2App);

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

    nodes.Get(6)->AddApplication(m1job2App);

    // 连接拥塞窗口跟踪
    n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job1));
    n1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n1_job1));
    n2job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n2_job1));
    n3job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n3_job1));
    n4job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n4_job1));
    m0job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_m0_job2));
    m1job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_m1_job2));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_n0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n4 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_m0 = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_m1 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n5 = staticRoutingHelper.GetStaticRouting(nodes.Get(7)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n6 = staticRoutingHelper.GetStaticRouting(nodes.Get(8)->GetObject<Ipv4>());

    staticRouting_n5->SetEnableAggregation(true);

    // 配置n0的路由表
    // n0到n6的路由:通过n5转发
    staticRouting_n0->AddHostRouteTo(i5i6.GetAddress(1), i0i5.GetAddress(1), 1);

    // 配置n1的路由表  
    // n1到n6的路由:通过n5转发
    staticRouting_n1->AddHostRouteTo(i5i6.GetAddress(1), i1i5.GetAddress(1), 1);

    // 配置n2的路由表
    // n2到n6的路由:通过n5转发
    staticRouting_n2->AddHostRouteTo(i5i6.GetAddress(1), i2i5.GetAddress(1), 1);

    // 配置n3的路由表
    // n3到n6的路由:通过n5转发
    staticRouting_n3->AddHostRouteTo(i5i6.GetAddress(1), i3i5.GetAddress(1), 1);

    // 配置n4的路由表
    // n4到n6的路由:通过n5转发
    staticRouting_n4->AddHostRouteTo(i5i6.GetAddress(1), i4i5.GetAddress(1), 1);

    // 配置m0的路由表
    // m0到n6的路由:通过n5转发
    staticRouting_m0->AddHostRouteTo(i5i6.GetAddress(1), im0i5.GetAddress(1), 1);

    // 配置m1的路由表
    // m1到n6的路由:通过n5转发
    staticRouting_m1->AddHostRouteTo(i5i6.GetAddress(1), im1i5.GetAddress(1), 1);

    // 配置n5的路由表
    // n5到n0的路由
    staticRouting_n5->AddHostRouteTo(i0i5.GetAddress(0), i0i5.GetAddress(0), 1);
    // n5到n1的路由  
    staticRouting_n5->AddHostRouteTo(i1i5.GetAddress(0), i1i5.GetAddress(0), 2);
    // n5到n2的路由
    staticRouting_n5->AddHostRouteTo(i2i5.GetAddress(0), i2i5.GetAddress(0), 3);
    // n5到n3的路由
    staticRouting_n5->AddHostRouteTo(i3i5.GetAddress(0), i3i5.GetAddress(0), 4);
    // n5到n4的路由
    staticRouting_n5->AddHostRouteTo(i4i5.GetAddress(0), i4i5.GetAddress(0), 5);
    // n5到m0的路由
    staticRouting_n5->AddHostRouteTo(im0i5.GetAddress(0), im0i5.GetAddress(0), 6);
    // n5到m1的路由
    staticRouting_n5->AddHostRouteTo(im1i5.GetAddress(0), im1i5.GetAddress(0), 7);
    // n5到n6的路由
    staticRouting_n5->AddHostRouteTo(i5i6.GetAddress(1), i5i6.GetAddress(1), 8);

    // 配置n6的路由表
    // n6到n0的路由:通过n5转发
    staticRouting_n6->AddHostRouteTo(i0i5.GetAddress(0), i5i6.GetAddress(0), 1);
    // n6到n1的路由:通过n5转发
    staticRouting_n6->AddHostRouteTo(i1i5.GetAddress(0), i5i6.GetAddress(0), 1);
    // n6到n2的路由:通过n5转发
    staticRouting_n6->AddHostRouteTo(i2i5.GetAddress(0), i5i6.GetAddress(0), 1);
    // n6到n3的路由:通过n5转发
    staticRouting_n6->AddHostRouteTo(i3i5.GetAddress(0), i5i6.GetAddress(0), 1);
    // n6到n4的路由:通过n5转发
    staticRouting_n6->AddHostRouteTo(i4i5.GetAddress(0), i5i6.GetAddress(0), 1);
    // n6到m0的路由:通过n5转发
    staticRouting_n6->AddHostRouteTo(im0i5.GetAddress(0), i5i6.GetAddress(0), 1);
    // n6到m1的路由:通过n5转发
    staticRouting_n6->AddHostRouteTo(im1i5.GetAddress(0), i5i6.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket->AddAddressMapping(1, i0i5.GetAddress(0), sendPort);  // job1 - n0
    sinkATPSocket->AddAddressMapping(1, i1i5.GetAddress(0), sendPort);  // job1 - n1
    sinkATPSocket->AddAddressMapping(1, i2i5.GetAddress(0), sendPort);  // job1 - n2
    sinkATPSocket->AddAddressMapping(1, i3i5.GetAddress(0), sendPort);  // job1 - n3
    sinkATPSocket->AddAddressMapping(1, i4i5.GetAddress(0), sendPort);  // job1 - n4
    sinkATPSocket->AddAddressMapping(2, im0i5.GetAddress(0), sendPort2); // job2 - m0
    sinkATPSocket->AddAddressMapping(2, im1i5.GetAddress(0), sendPort2); // job2 - m1

    // 开始测量
    Simulator::Schedule(Seconds(0.9), &Measurement, sinkApp);
    
    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    cwndStream_n0_job1.close();
    cwndStream_n1_job1.close();
    cwndStream_n2_job1.close();
    cwndStream_n3_job1.close();
    cwndStream_n4_job1.close();
    cwndStream_m0_job2.close();
    cwndStream_m1_job2.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    queueSizeStream_n5.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
