/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// n0,n1(job1), m0,m1(job2), a0,a1(job3) and b0,b1(job4) to n2, n2 to n3
//
// - Flow from n0,n1(job1), m0,m1(job2), a0,a1(job3) and b0,b1(job4) to n3 using BulkSendApplication.
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

NS_LOG_COMPONENT_DEFINE("ATP-P2P-4job");

// 拥塞窗口跟踪，写入txt文件
ofstream cwndStream_n0_job1;
ofstream cwndStream_n1_job1;
ofstream cwndStream_m0_job2;
ofstream cwndStream_m1_job2;
ofstream cwndStream_a0_job3;
ofstream cwndStream_a1_job3;
ofstream cwndStream_b0_job4;
ofstream cwndStream_b1_job4;

// 添加队列长度跟踪文件流
ofstream queueSizeStream_n2;
uint32_t lastQueueSize = 0;  // 用于存储上一次记录的队列大小

// PacketSink端接收到的job1、job2、job3和job4的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;
uint64_t lastTimeJob3Bytes = 0;
uint64_t lastTimeJob4Bytes = 0;

ofstream SinkBytesStream_job1;
ofstream SinkBytesStream_job2;
ofstream SinkBytesStream_job3;
ofstream SinkBytesStream_job4;

// 记录接收端收到的总字节数
static void
Measurement(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = sink->GetTotalRxJob(1);  // job1
    uint64_t currentTimeJob2Bytes = sink->GetTotalRxJob(2);  // job2
    uint64_t currentTimeJob3Bytes = sink->GetTotalRxJob(3);  // job3
    uint64_t currentTimeJob4Bytes = sink->GetTotalRxJob(4);  // job4
        
    // 100us内接收到的字节数
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;
    uint64_t ReceivedJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;
    uint64_t ReceivedJob3BytesPer100ms = currentTimeJob3Bytes - lastTimeJob3Bytes;
    uint64_t ReceivedJob4BytesPer100ms = currentTimeJob4Bytes - lastTimeJob4Bytes;

    // 记录总字节数和本100us内接收的字节数
    SinkBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    SinkBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << ReceivedJob2BytesPer100ms << std::endl;
    SinkBytesStream_job3 << now.GetMicroSeconds() << "\t" << currentTimeJob3Bytes << "\t" << ReceivedJob3BytesPer100ms << std::endl;
    SinkBytesStream_job4 << now.GetMicroSeconds() << "\t" << currentTimeJob4Bytes << "\t" << ReceivedJob4BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
    lastTimeJob2Bytes = currentTimeJob2Bytes;
    lastTimeJob3Bytes = currentTimeJob3Bytes;
    lastTimeJob4Bytes = currentTimeJob4Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &Measurement, sink);
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
CwndChange_m0_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_m0_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_m1_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_m1_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_a0_job3(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_a0_job3 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_a1_job3(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_a1_job3 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_b0_job4(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_b0_job4 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_b1_job4(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_b1_job4 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

// 修改队列长度跟踪回调函数为定期采样
static void
SampleQueueSize(Ptr<PointToPointNetDevice> device)
{
    uint32_t currentSize = device->GetQueue()->GetNPackets();
    queueSizeStream_n2 << Simulator::Now().GetMicroSeconds() << "\t" << currentSize << std::endl;
    
    // 调度下一次采样
    Simulator::Schedule(MicroSeconds(10), &SampleQueueSize, device);
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
    cwndStream_n0_job1.open("atp-result/trace-p2p-4job/n0-job1-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    cwndStream_n1_job1.open("atp-result/trace-p2p-4job/n1-job1-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    cwndStream_m0_job2.open("atp-result/trace-p2p-4job/m0-job2-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    cwndStream_m1_job2.open("atp-result/trace-p2p-4job/m1-job2-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    cwndStream_a0_job3.open("atp-result/trace-p2p-4job/a0-job3-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    cwndStream_a1_job3.open("atp-result/trace-p2p-4job/a1-job3-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    cwndStream_b0_job4.open("atp-result/trace-p2p-4job/b0-job4-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    cwndStream_b1_job4.open("atp-result/trace-p2p-4job/b1-job4-cwnd-p2p-4job.txt", ofstream::out | ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-p2p-4job/job1-sinkBytes-p2p-4job.txt", ofstream::out | ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-p2p-4job/job2-sinkBytes-p2p-4job.txt", ofstream::out | ofstream::trunc);
    SinkBytesStream_job3.open("atp-result/trace-p2p-4job/job3-sinkBytes-p2p-4job.txt", ofstream::out | ofstream::trunc);
    SinkBytesStream_job4.open("atp-result/trace-p2p-4job/job4-sinkBytes-p2p-4job.txt", ofstream::out | ofstream::trunc);
    queueSizeStream_n2.open("atp-result/trace-p2p-4job/n2-queueSize-p2p-4job.txt", ofstream::out | ofstream::trunc);
    

    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(10000); // 9.6us为一个rtt时间，近似为10us，这里跑2000个rtt

    // 设置job1、job2、job3和job4初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;
    uint32_t job3_initCwnd = 1;
    uint32_t job4_initCwnd = 1;

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_n0_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_n1_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_m0_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;
    cwndStream_m1_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;
    cwndStream_a0_job3 << initialTimestamp << "\t" << job3_initCwnd << std::endl;
    cwndStream_a1_job3 << initialTimestamp << "\t" << job3_initCwnd << std::endl;
    cwndStream_b0_job4 << initialTimestamp << "\t" << job4_initCwnd << std::endl;
    cwndStream_b1_job4 << initialTimestamp << "\t" << job4_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(10); // 10个节点: n0,n1,m0,m1,a0,a1,b0,b1,n2,n3

    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(8)); // n0-n2
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(8)); // n1-n2
    NodeContainer m0n2 = NodeContainer(nodes.Get(2), nodes.Get(8)); // m0-n2
    NodeContainer m1n2 = NodeContainer(nodes.Get(3), nodes.Get(8)); // m1-n2
    NodeContainer a0n2 = NodeContainer(nodes.Get(4), nodes.Get(8)); // a0-n2
    NodeContainer a1n2 = NodeContainer(nodes.Get(5), nodes.Get(8)); // a1-n2
    NodeContainer b0n2 = NodeContainer(nodes.Get(6), nodes.Get(8)); // b0-n2
    NodeContainer b1n2 = NodeContainer(nodes.Get(7), nodes.Get(8)); // b1-n2
    NodeContainer n2n3 = NodeContainer(nodes.Get(8), nodes.Get(9)); // n2-n3

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));

    NetDeviceContainer d0d2, d1d2, m0d2, m1d2, a0d2, a1d2, b0d2, b1d2, d2d3;
    
    // Install n0,n1 to n2 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    d0d2 = pointToPoint.Install(n0n2);
    d1d2 = pointToPoint.Install(n1n2);

    // Install m0,m1 to n2 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    m0d2 = pointToPoint.Install(m0n2);
    m1d2 = pointToPoint.Install(m1n2);

    // Install a0,a1 to n2 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    a0d2 = pointToPoint.Install(a0n2);
    a1d2 = pointToPoint.Install(a1n2);

    // Install b0,b1 to n2 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    b0d2 = pointToPoint.Install(b0n2);
    b1d2 = pointToPoint.Install(b1n2);

    // Install n2 to n3 link
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    d2d3 = pointToPoint.Install(n2n3);

    // 设置n2上与n3连接部分的队列阈值
    Ptr<PointToPointNetDevice> n2Device = DynamicCast<PointToPointNetDevice>(d2d3.Get(0));
    n2Device->SetThreshold(80);
    
    // 启动队列长度采样
    Simulator::Schedule(Seconds(1.0), &SampleQueueSize, n2Device);

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
    Ipv4InterfaceContainer im0i2 = ipv4Helper.Assign(m0d2);

    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer im1i2 = ipv4Helper.Assign(m1d2);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ia0i2 = ipv4Helper.Assign(a0d2);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ia1i2 = ipv4Helper.Assign(a1d2);

    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer ib0i2 = ipv4Helper.Assign(b0d2);

    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer ib1i2 = ipv4Helper.Assign(b1d2);

    ipv4Helper.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4Helper.Assign(d2d3);

    //
    // Create a PacketSinkApplication and install it on node 9 (n3)
    //
    NS_LOG_INFO("Create Applications.");

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();

    // add address and port
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort));
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
    // Create sockets for job1, job2, job3 and job4
    //
    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> m0job2socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> m1job2socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());
    Ptr<Socket> a0job3socket = Socket::CreateSocket(nodes.Get(4), ATPSocketFactory::GetTypeId());
    Ptr<Socket> a1job3socket = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId());
    Ptr<Socket> b0job4socket = Socket::CreateSocket(nodes.Get(6), ATPSocketFactory::GetTypeId());
    Ptr<Socket> b1job4socket = Socket::CreateSocket(nodes.Get(7), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> n0job1_ATPSocket = DynamicCast<ATPSocket>(n0job1socket);
    Ptr<ATPSocket> n1job1_ATPSocket = DynamicCast<ATPSocket>(n1job1socket);
    Ptr<ATPSocket> m0job2_ATPSocket = DynamicCast<ATPSocket>(m0job2socket);
    Ptr<ATPSocket> m1job2_ATPSocket = DynamicCast<ATPSocket>(m1job2socket);
    Ptr<ATPSocket> a0job3_ATPSocket = DynamicCast<ATPSocket>(a0job3socket);
    Ptr<ATPSocket> a1job3_ATPSocket = DynamicCast<ATPSocket>(a1job3socket);
    Ptr<ATPSocket> b0job4_ATPSocket = DynamicCast<ATPSocket>(b0job4socket);
    Ptr<ATPSocket> b1job4_ATPSocket = DynamicCast<ATPSocket>(b1job4socket);

    // Create and configure applications
    Ptr<ATPBulkSendApplication> n0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> m0job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> m1job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> a0job3App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> a1job3App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> b0job4App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> b1job4App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort = 11;  // Port for job1
    uint16_t sendPort2 = 12; // Port for job2
    uint16_t sendPort3 = 13; // Port for job3
    uint16_t sendPort4 = 14; // Port for job4

    Address n0Address(InetSocketAddress(i0i2.GetAddress(0), sendPort));  // 使用端口11
    Address n1Address(InetSocketAddress(i1i2.GetAddress(0), sendPort));  // 使用端口11
    Address m0Address(InetSocketAddress(im0i2.GetAddress(0), sendPort2));  // 使用端口12
    Address m1Address(InetSocketAddress(im1i2.GetAddress(0), sendPort2));  // 使用端口12
    Address a0Address(InetSocketAddress(ia0i2.GetAddress(0), sendPort3));  // 使用端口13
    Address a1Address(InetSocketAddress(ia1i2.GetAddress(0), sendPort3));  // 使用端口13
    Address b0Address(InetSocketAddress(ib0i2.GetAddress(0), sendPort4));  // 使用端口14
    Address b1Address(InetSocketAddress(ib1i2.GetAddress(0), sendPort4));  // 使用端口14

    // 设置job1初始拥塞窗口
    n0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    n1job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    
    // Configure n0job1App
    n0job1App->Setup(sinkAddress, n0job1_ATPSocket, maxBytes, 1);
    n0job1App->SetEnableATPTag(true);
    n0job1App->SetStartTime(Seconds(1.0));
    n0job1App->SetStopTime(stopTime);
    n0job1App->SetFaninDegree(0b00000011); // 2个发送端
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
    n1job1App->SetFaninDegree(0b00000011); // 2个发送端
    n1job1App->SetWorkerId(0b00000010);

    n1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n1job1App));
    n1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n1job1App));
    n1job1_ATPSocket->Bind(n1Address);
    n1job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(1)->AddApplication(n1job1App);

    // 设置job2初始拥塞窗口
    m0job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    m1job2_ATPSocket->SetInitCwnd(job2_initCwnd);

    // Configure m0job2App
    m0job2App->Setup(sinkAddress, m0job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    m0job2App->SetEnableATPTag(true);
    m0job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    m0job2App->SetStopTime(stopTime);
    m0job2App->SetFaninDegree(0b00000011); // 2个发送端
    m0job2App->SetWorkerId(0b00000001);

    m0job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, m0job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, m0job2App));
    m0job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, m0job2App));
    m0job2_ATPSocket->Bind(m0Address);
    m0job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(2)->AddApplication(m0job2App);

    // Configure m1job2App
    m1job2App->Setup(sinkAddress, m1job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    m1job2App->SetEnableATPTag(true);
    m1job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    m1job2App->SetStopTime(stopTime);
    m1job2App->SetFaninDegree(0b00000011); // 2个发送端
    m1job2App->SetWorkerId(0b00000010);

    m1job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, m1job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, m1job2App));
    m1job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, m1job2App));
    m1job2_ATPSocket->Bind(m1Address);
    m1job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(3)->AddApplication(m1job2App);

    // 设置job3初始拥塞窗口
    a0job3_ATPSocket->SetInitCwnd(job3_initCwnd);
    a1job3_ATPSocket->SetInitCwnd(job3_initCwnd);

    // Configure a0job3App
    a0job3App->Setup(sinkAddress, a0job3_ATPSocket, maxBytes, 3);  // Note jobId = 3
    a0job3App->SetEnableATPTag(true);
    a0job3App->SetStartTime(Seconds(1.0));  // Start at the same time as other jobs
    a0job3App->SetStopTime(stopTime);
    a0job3App->SetFaninDegree(0b00000011); // 2个发送端
    a0job3App->SetWorkerId(0b00000001);

    a0job3_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, a0job3App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, a0job3App));
    a0job3_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, a0job3App));
    a0job3_ATPSocket->Bind(a0Address);
    a0job3_ATPSocket->Connect(sinkAddress);

    nodes.Get(4)->AddApplication(a0job3App);

    // Configure a1job3App
    a1job3App->Setup(sinkAddress, a1job3_ATPSocket, maxBytes, 3);  // Note jobId = 3
    a1job3App->SetEnableATPTag(true);
    a1job3App->SetStartTime(Seconds(1.0));  // Start at the same time as other jobs
    a1job3App->SetStopTime(stopTime);
    a1job3App->SetFaninDegree(0b00000011); // 2个发送端
    a1job3App->SetWorkerId(0b00000010);

    a1job3_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, a1job3App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, a1job3App));
    a1job3_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, a1job3App));
    a1job3_ATPSocket->Bind(a1Address);
    a1job3_ATPSocket->Connect(sinkAddress);

    nodes.Get(5)->AddApplication(a1job3App);

    // 设置job4初始拥塞窗口
    b0job4_ATPSocket->SetInitCwnd(job4_initCwnd);
    b1job4_ATPSocket->SetInitCwnd(job4_initCwnd);

    // Configure b0job4App
    b0job4App->Setup(sinkAddress, b0job4_ATPSocket, maxBytes, 4);  // Note jobId = 4
    b0job4App->SetEnableATPTag(true);
    b0job4App->SetStartTime(Seconds(1.0));  // Start at the same time as other jobs
    b0job4App->SetStopTime(stopTime);
    b0job4App->SetFaninDegree(0b00000011); // 2个发送端
    b0job4App->SetWorkerId(0b00000001);

    b0job4_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, b0job4App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, b0job4App));
    b0job4_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, b0job4App));
    b0job4_ATPSocket->Bind(b0Address);
    b0job4_ATPSocket->Connect(sinkAddress);

    nodes.Get(6)->AddApplication(b0job4App);

    // Configure b1job4App
    b1job4App->Setup(sinkAddress, b1job4_ATPSocket, maxBytes, 4);  // Note jobId = 4
    b1job4App->SetEnableATPTag(true);
    b1job4App->SetStartTime(Seconds(1.0));  // Start at the same time as other jobs
    b1job4App->SetStopTime(stopTime);
    b1job4App->SetFaninDegree(0b00000011); // 2个发送端
    b1job4App->SetWorkerId(0b00000010);

    b1job4_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, b1job4App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, b1job4App));
    b1job4_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, b1job4App));
    b1job4_ATPSocket->Bind(b1Address);
    b1job4_ATPSocket->Connect(sinkAddress);

    nodes.Get(7)->AddApplication(b1job4App);

    // 连接拥塞窗口跟踪
    n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job1));
    n1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n1_job1));
    m0job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_m0_job2));
    m1job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_m1_job2));
    a0job3_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_a0_job3));
    a1job3_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_a1_job3));
    b0job4_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_b0_job4));
    b1job4_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_b1_job4));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_n0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_m0 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_m1 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_a0 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_a1 = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_b0 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_b1 = staticRoutingHelper.GetStaticRouting(nodes.Get(7)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n2 = staticRoutingHelper.GetStaticRouting(nodes.Get(8)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n3 = staticRoutingHelper.GetStaticRouting(nodes.Get(9)->GetObject<Ipv4>());

    staticRouting_n2->SetEnableAggregation(true);

    // 配置n0的路由表
    // n0到n3的路由:通过n2转发
    staticRouting_n0->AddHostRouteTo(i2i3.GetAddress(1), i0i2.GetAddress(1), 1);

    // 配置n1的路由表  
    // n1到n3的路由:通过n2转发
    staticRouting_n1->AddHostRouteTo(i2i3.GetAddress(1), i1i2.GetAddress(1), 1);

    // 配置m0的路由表
    // m0到n3的路由:通过n2转发
    staticRouting_m0->AddHostRouteTo(i2i3.GetAddress(1), im0i2.GetAddress(1), 1);

    // 配置m1的路由表
    // m1到n3的路由:通过n2转发
    staticRouting_m1->AddHostRouteTo(i2i3.GetAddress(1), im1i2.GetAddress(1), 1);

    // 配置a0的路由表
    // a0到n3的路由:通过n2转发
    staticRouting_a0->AddHostRouteTo(i2i3.GetAddress(1), ia0i2.GetAddress(1), 1);

    // 配置a1的路由表
    // a1到n3的路由:通过n2转发
    staticRouting_a1->AddHostRouteTo(i2i3.GetAddress(1), ia1i2.GetAddress(1), 1);

    // 配置b0的路由表
    // b0到n3的路由:通过n2转发
    staticRouting_b0->AddHostRouteTo(i2i3.GetAddress(1), ib0i2.GetAddress(1), 1);

    // 配置b1的路由表
    // b1到n3的路由:通过n2转发
    staticRouting_b1->AddHostRouteTo(i2i3.GetAddress(1), ib1i2.GetAddress(1), 1);

    // 配置n2的路由表
    // n2到n0的路由
    staticRouting_n2->AddHostRouteTo(i0i2.GetAddress(0), i0i2.GetAddress(0), 1);
    // n2到n1的路由  
    staticRouting_n2->AddHostRouteTo(i1i2.GetAddress(0), i1i2.GetAddress(0), 2);
    // n2到m0的路由
    staticRouting_n2->AddHostRouteTo(im0i2.GetAddress(0), im0i2.GetAddress(0), 3);
    // n2到m1的路由
    staticRouting_n2->AddHostRouteTo(im1i2.GetAddress(0), im1i2.GetAddress(0), 4);
    // n2到a0的路由
    staticRouting_n2->AddHostRouteTo(ia0i2.GetAddress(0), ia0i2.GetAddress(0), 5);
    // n2到a1的路由
    staticRouting_n2->AddHostRouteTo(ia1i2.GetAddress(0), ia1i2.GetAddress(0), 6);
    // n2到b0的路由
    staticRouting_n2->AddHostRouteTo(ib0i2.GetAddress(0), ib0i2.GetAddress(0), 7);
    // n2到b1的路由
    staticRouting_n2->AddHostRouteTo(ib1i2.GetAddress(0), ib1i2.GetAddress(0), 8);
    // n2到n3的路由
    staticRouting_n2->AddHostRouteTo(i2i3.GetAddress(1), i2i3.GetAddress(1), 9);

    // 配置n3的路由表
    // n3到n0的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(i0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到n1的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(i1i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到m0的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(im0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到m1的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(im1i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到a0的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(ia0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到a1的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(ia1i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到b0的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(ib0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到b1的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(ib1i2.GetAddress(0), i2i3.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket->AddAddressMapping(1, i0i2.GetAddress(0), sendPort);  // job1 - n0
    sinkATPSocket->AddAddressMapping(1, i1i2.GetAddress(0), sendPort);  // job1 - n1
    sinkATPSocket->AddAddressMapping(2, im0i2.GetAddress(0), sendPort2); // job2 - m0
    sinkATPSocket->AddAddressMapping(2, im1i2.GetAddress(0), sendPort2); // job2 - m1
    sinkATPSocket->AddAddressMapping(3, ia0i2.GetAddress(0), sendPort3); // job3 - a0
    sinkATPSocket->AddAddressMapping(3, ia1i2.GetAddress(0), sendPort3); // job3 - a1
    sinkATPSocket->AddAddressMapping(4, ib0i2.GetAddress(0), sendPort4); // job4 - b0
    sinkATPSocket->AddAddressMapping(4, ib1i2.GetAddress(0), sendPort4); // job4 - b1

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
    cwndStream_n1_job1.close();
    cwndStream_m0_job2.close();
    cwndStream_m1_job2.close();
    cwndStream_a0_job3.close();
    cwndStream_a1_job3.close();
    cwndStream_b0_job4.close();
    cwndStream_b1_job4.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();
    SinkBytesStream_job3.close();
    SinkBytesStream_job4.close();
    queueSizeStream_n2.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
