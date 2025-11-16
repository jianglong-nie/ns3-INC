/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
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
std::ofstream cwndStream_job2;

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

std::ofstream sendBytesStream_job1;
std::ofstream sendBytesStream_job2;

// 记录发送端发送的总字节数
static void
MeasurementTxJob1(Ptr<ATPSocket> socket)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = socket->GetTotalTxBytes();  // job1
        
    // 100us内发送的字节数
    uint64_t SendJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;

    // 记录总字节数和本100us内发送的字节数
    sendBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << SendJob1BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &MeasurementTxJob1, socket);
}

static void
MeasurementTxJob2(Ptr<ATPSocket> socket)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob2Bytes = socket->GetTotalTxBytes();  // job2

    // 100us内接收到的字节数
    uint64_t SendJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;

    // 记录总字节数和本100us内发送的字节数
    sendBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << SendJob2BytesPer100ms << std::endl;
    
    lastTimeJob2Bytes = currentTimeJob2Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &MeasurementTxJob2, socket);
}

static void
CwndChange_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

int
main(int argc, char* argv[])
{
    // 日志配置（可根据需要启用）
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    
    // 注意：超时重传机制已启用
    // 默认超时时间：500微秒，检查间隔：100微秒
    // 可通过下方参数调整

    cwndStream_job1.open("atp-result/trace-ha-twojobs/job1-cwnd-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_job2.open("atp-result/trace-ha-twojobs/job2-cwnd-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_job1.open("atp-result/trace-ha-twojobs/job1-sendBytes-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_job2.open("atp-result/trace-ha-twojobs/job2-sendBytes-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);

    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(10000); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;
    
    // 设置超时重传参数
    // 当数据包发送后超过retxTimeout时间未收到ACK，将触发重传
    // retxCheckInterval是定期检查超时的间隔时间
    Time retxTimeout = MicroSeconds(12);         // 重传超时时间：12us（可调整）
    Time retxCheckInterval = MicroSeconds(6);   // 超时检查间隔：6us（可调整）

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(13); // 节点: w0,w1,w2,w3,w4,w5,w6,w7,s0,s1,s2,ps1,ps2
    // job1: w0, w1, w4, w5
    // job2: w2, w3, w6, w7

    NodeContainer w0s0 = NodeContainer(nodes.Get(0), nodes.Get(8));
    NodeContainer w1s0 = NodeContainer(nodes.Get(1), nodes.Get(8));
    NodeContainer w2s0 = NodeContainer(nodes.Get(2), nodes.Get(8));
    NodeContainer w3s0 = NodeContainer(nodes.Get(3), nodes.Get(8));

    NodeContainer w4s1 = NodeContainer(nodes.Get(4), nodes.Get(9));
    NodeContainer w5s1 = NodeContainer(nodes.Get(5), nodes.Get(9));
    NodeContainer w6s1 = NodeContainer(nodes.Get(6), nodes.Get(9));
    NodeContainer w7s1 = NodeContainer(nodes.Get(7), nodes.Get(9));

    NodeContainer s0s2 = NodeContainer(nodes.Get(8), nodes.Get(10));
    NodeContainer s1s2 = NodeContainer(nodes.Get(9), nodes.Get(10));

    NodeContainer s2ps1 = NodeContainer(nodes.Get(10), nodes.Get(11));
    NodeContainer s2ps2 = NodeContainer(nodes.Get(10), nodes.Get(12));

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1us"));

    NetDeviceContainer dev_w0s0, dev_w1s0, dev_w2s0, dev_w3s0;
    NetDeviceContainer dev_w4s1, dev_w5s1, dev_w6s1, dev_w7s1;
    NetDeviceContainer dev_s0s2, dev_s1s2, dev_s2ps1, dev_s2ps2;

    dev_w0s0 = pointToPoint.Install(w0s0);
    dev_w1s0 = pointToPoint.Install(w1s0);
    dev_w2s0 = pointToPoint.Install(w2s0);
    dev_w3s0 = pointToPoint.Install(w3s0);

    dev_w4s1 = pointToPoint.Install(w4s1);
    dev_w5s1 = pointToPoint.Install(w5s1);
    dev_w6s1 = pointToPoint.Install(w6s1);
    dev_w7s1 = pointToPoint.Install(w7s1);

    dev_s0s2 = pointToPoint.Install(s0s2);
    dev_s1s2 = pointToPoint.Install(s1s2);

    dev_s2ps1 = pointToPoint.Install(s2ps1);
    dev_s2ps2 = pointToPoint.Install(s2ps2);

    
    // 设置s0s2, s1s2, s2ps的队列阈值
    Ptr<PointToPointNetDevice> s0s2Device = DynamicCast<PointToPointNetDevice>(dev_s0s2.Get(0));
    Ptr<PointToPointNetDevice> s1s2Device = DynamicCast<PointToPointNetDevice>(dev_s1s2.Get(0));
    Ptr<PointToPointNetDevice> s2ps1Device = DynamicCast<PointToPointNetDevice>(dev_s2ps1.Get(0));
    Ptr<PointToPointNetDevice> s2ps2Device = DynamicCast<PointToPointNetDevice>(dev_s2ps2.Get(0));
    NS_ASSERT(s0s2Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s2Device != nullptr); // 确保转换成功
    NS_ASSERT(s2ps1Device != nullptr); // 确保转换成功
    NS_ASSERT(s2ps2Device != nullptr); // 确保转换成功
    /*
    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p (大于100)

    // 直接在这个设备上设置自定义队列
    n2Device->SetQueue(customQueue);
    */

    s0s2Device->SetThreshold(160);
    s1s2Device->SetThreshold(160);
    s2ps1Device->SetThreshold(160);
    s2ps2Device->SetThreshold(160);

    s0s2Device->SetEnableEcn(false);
    s1s2Device->SetEnableEcn(false);
    s2ps1Device->SetEnableEcn(false);
    s2ps2Device->SetEnableEcn(false);

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
    Ipv4InterfaceContainer ip_w0s0 = ipv4Helper.Assign(dev_w0s0);

    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w1s0 = ipv4Helper.Assign(dev_w1s0);

    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w2s0 = ipv4Helper.Assign(dev_w2s0);

    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w3s0 = ipv4Helper.Assign(dev_w3s0);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w4s1 = ipv4Helper.Assign(dev_w4s1);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w5s1 = ipv4Helper.Assign(dev_w5s1);

    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w6s1 = ipv4Helper.Assign(dev_w6s1);

    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w7s1 = ipv4Helper.Assign(dev_w7s1);

    ipv4Helper.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s2 = ipv4Helper.Assign(dev_s0s2);

    ipv4Helper.SetBase("10.1.10.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s2 = ipv4Helper.Assign(dev_s1s2);

    ipv4Helper.SetBase("10.1.11.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2ps1 = ipv4Helper.Assign(dev_s2ps1);

    ipv4Helper.SetBase("10.1.12.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2ps2 = ipv4Helper.Assign(dev_s2ps2);

    //
    // Create a PacketSinkApplication and install it on node 3
    //
    NS_LOG_INFO("Create Applications.");

    // Create sink applications for both jobs
    Ptr<ATPPacketSink> sinkApp1 = CreateObject<ATPPacketSink>();
    Ptr<ATPPacketSink> sinkApp2 = CreateObject<ATPPacketSink>();

    // job1 sink (ps1)
    uint16_t sinkPort1 = 9;
    Address sinkAddress1(InetSocketAddress(ip_s2ps1.GetAddress(1), sinkPort1));
    sinkApp1->SetAddressPort(sinkAddress1, sinkPort1);

    // job2 sink (ps2)
    uint16_t sinkPort2 = 9;
    Address sinkAddress2(InetSocketAddress(ip_s2ps2.GetAddress(1), sinkPort2));
    sinkApp2->SetAddressPort(sinkAddress2, sinkPort2);

    // create ATPSocket for sink1 and bind to sinkAddress1
    Ptr<Socket> sinkSocket1 = Socket::CreateSocket(nodes.Get(11), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket1 = DynamicCast<ATPSocket>(sinkSocket1);
    sinkApp1->SetSocket(sinkATPSocket1);
    sinkATPSocket1->Bind(sinkAddress1);
    sinkATPSocket1->Listen();

    // create ATPSocket for sink2 and bind to sinkAddress2
    Ptr<Socket> sinkSocket2 = Socket::CreateSocket(nodes.Get(12), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket2 = DynamicCast<ATPSocket>(sinkSocket2);
    sinkApp2->SetSocket(sinkATPSocket2);
    sinkATPSocket2->Bind(sinkAddress2);
    sinkATPSocket2->Listen();

    // start sink applications
    sinkApp1->SetStartTime(Seconds(0.0));
    sinkApp1->SetStopTime(stopTime);
    nodes.Get(11)->AddApplication(sinkApp1);  // ps1

    sinkApp2->SetStartTime(Seconds(0.0));
    sinkApp2->SetStopTime(stopTime);
    nodes.Get(12)->AddApplication(sinkApp2);  // ps2

    //
    // Create sockets for job1 (w0, w1, w4, w5)
    //
    Ptr<Socket> w0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w4job1socket = Socket::CreateSocket(nodes.Get(4), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w5job1socket = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w0job1_ATPSocket = DynamicCast<ATPSocket>(w0job1socket);
    Ptr<ATPSocket> w1job1_ATPSocket = DynamicCast<ATPSocket>(w1job1socket);
    Ptr<ATPSocket> w4job1_ATPSocket = DynamicCast<ATPSocket>(w4job1socket);
    Ptr<ATPSocket> w5job1_ATPSocket = DynamicCast<ATPSocket>(w5job1socket);

    //
    // Create sockets for job2 (w2, w3, w6, w7)
    //
    Ptr<Socket> w2job2socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w3job2socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w6job2socket = Socket::CreateSocket(nodes.Get(6), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w7job2socket = Socket::CreateSocket(nodes.Get(7), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w2job2_ATPSocket = DynamicCast<ATPSocket>(w2job2socket);
    Ptr<ATPSocket> w3job2_ATPSocket = DynamicCast<ATPSocket>(w3job2socket);
    Ptr<ATPSocket> w6job2_ATPSocket = DynamicCast<ATPSocket>(w6job2socket);
    Ptr<ATPSocket> w7job2_ATPSocket = DynamicCast<ATPSocket>(w7job2socket);

    // Create applications for job1
    Ptr<ATPBulkSendApplication> w0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w4job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w5job1App = CreateObject<ATPBulkSendApplication>();

    // Create applications for job2
    Ptr<ATPBulkSendApplication> w2job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w3job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w6job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w7job2App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort1 = 11;  // Port for job1
    uint16_t sendPort2 = 12;  // Port for job2

    // Addresses for job1 workers
    Address w0Address(InetSocketAddress(ip_w0s0.GetAddress(0), sendPort1));
    Address w1Address(InetSocketAddress(ip_w1s0.GetAddress(0), sendPort1));
    Address w4Address(InetSocketAddress(ip_w4s1.GetAddress(0), sendPort1));
    Address w5Address(InetSocketAddress(ip_w5s1.GetAddress(0), sendPort1));

    // Addresses for job2 workers
    Address w2Address(InetSocketAddress(ip_w2s0.GetAddress(0), sendPort2));
    Address w3Address(InetSocketAddress(ip_w3s0.GetAddress(0), sendPort2));
    Address w6Address(InetSocketAddress(ip_w6s1.GetAddress(0), sendPort2));
    Address w7Address(InetSocketAddress(ip_w7s1.GetAddress(0), sendPort2));

    // 设置job1初始拥塞窗口和超时参数
    w0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w0job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w0job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w1job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w1job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w1job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w4job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w4job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w4job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w5job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w5job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w5job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);

    // 设置job2初始拥塞窗口和超时参数
    w2job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w2job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w2job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w3job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w3job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w3job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w6job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w6job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w6job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w7job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w7job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w7job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    // Configure w0job1App
    uint8_t job1Id = 1;
    w0job1App->Setup(sinkAddress1, w0job1_ATPSocket, maxBytes, job1Id);
    w0job1App->SetEnableATPTag(true);
    w0job1App->SetStartTime(Seconds(1.0));
    w0job1App->SetStopTime(stopTime);
    w0job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w0job1App->SetWorkerId(0b00000001);

    w0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w0job1App));
    w0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w0job1App));
    w0job1_ATPSocket->Bind(w0Address);
    w0job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(0)->AddApplication(w0job1App);

    // Configure w1job1App
    w1job1App->Setup(sinkAddress1, w1job1_ATPSocket, maxBytes, job1Id);
    w1job1App->SetEnableATPTag(true);
    w1job1App->SetStartTime(Seconds(1.0));
    w1job1App->SetStopTime(stopTime);
    w1job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w1job1App->SetWorkerId(0b00000010);

    w1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w1job1App));
    w1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w1job1App));
    w1job1_ATPSocket->Bind(w1Address);
    w1job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(1)->AddApplication(w1job1App);

    // Configure w4job1App
    w4job1App->Setup(sinkAddress1, w4job1_ATPSocket, maxBytes, job1Id);
    w4job1App->SetEnableATPTag(true);
    w4job1App->SetStartTime(Seconds(1.0));
    w4job1App->SetStopTime(stopTime);
    w4job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w4job1App->SetWorkerId(0b00000100);

    w4job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w4job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w4job1App));
    w4job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w4job1App));
    w4job1_ATPSocket->Bind(w4Address);
    w4job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(4)->AddApplication(w4job1App);

    // Configure w5job1App
    w5job1App->Setup(sinkAddress1, w5job1_ATPSocket, maxBytes, job1Id);
    w5job1App->SetEnableATPTag(true);
    w5job1App->SetStartTime(Seconds(1.0));
    w5job1App->SetStopTime(stopTime);
    w5job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w5job1App->SetWorkerId(0b00001000);

    w5job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w5job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w5job1App));
    w5job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w5job1App));
    w5job1_ATPSocket->Bind(w5Address);
    w5job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(5)->AddApplication(w5job1App);  // 修正：w5应该安装到节点5

    // Configure w2job2App
    uint8_t job2Id = 2;
    w2job2App->Setup(sinkAddress2, w2job2_ATPSocket, maxBytes, job2Id);
    w2job2App->SetEnableATPTag(true);
    w2job2App->SetStartTime(Seconds(1.0));
    w2job2App->SetStopTime(stopTime);
    w2job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w2job2App->SetWorkerId(0b00000001);

    w2job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w2job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w2job2App));
    w2job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w2job2App));
    w2job2_ATPSocket->Bind(w2Address);
    w2job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(2)->AddApplication(w2job2App);

    // Configure w3job2App
    w3job2App->Setup(sinkAddress2, w3job2_ATPSocket, maxBytes, job2Id);
    w3job2App->SetEnableATPTag(true);
    w3job2App->SetStartTime(Seconds(1.0));
    w3job2App->SetStopTime(stopTime);
    w3job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w3job2App->SetWorkerId(0b00000010);

    w3job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w3job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w3job2App));
    w3job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w3job2App));
    w3job2_ATPSocket->Bind(w3Address);
    w3job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(3)->AddApplication(w3job2App);

    // Configure w6job2App
    w6job2App->Setup(sinkAddress2, w6job2_ATPSocket, maxBytes, job2Id);
    w6job2App->SetEnableATPTag(true);
    w6job2App->SetStartTime(Seconds(1.0));
    w6job2App->SetStopTime(stopTime);
    w6job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w6job2App->SetWorkerId(0b00000100);

    w6job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w6job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w6job2App));
    w6job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w6job2App));
    w6job2_ATPSocket->Bind(w6Address);
    w6job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(6)->AddApplication(w6job2App);

    // Configure w7job2App
    w7job2App->Setup(sinkAddress2, w7job2_ATPSocket, maxBytes, job2Id);
    w7job2App->SetEnableATPTag(true);
    w7job2App->SetStartTime(Seconds(1.0));
    w7job2App->SetStopTime(stopTime);
    w7job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w7job2App->SetWorkerId(0b00001000);

    w7job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w7job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w7job2App));
    w7job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w7job2App));
    w7job2_ATPSocket->Bind(w7Address);
    w7job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(7)->AddApplication(w7job2App);

    // 连接拥塞窗口跟踪
    w0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_job1));
    w2job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_job2));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_w0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w4 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w5 = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w6 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w7 = staticRoutingHelper.GetStaticRouting(nodes.Get(7)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s0 = staticRoutingHelper.GetStaticRouting(nodes.Get(8)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s1 = staticRoutingHelper.GetStaticRouting(nodes.Get(9)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s2 = staticRoutingHelper.GetStaticRouting(nodes.Get(10)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps1 = staticRoutingHelper.GetStaticRouting(nodes.Get(11)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps2 = staticRoutingHelper.GetStaticRouting(nodes.Get(12)->GetObject<Ipv4>());
    
    staticRouting_s0->SetEnableAggregation(true);
    staticRouting_s1->SetEnableAggregation(true);
    staticRouting_s2->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> atpl4_s0 = staticRouting_s0->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s1 = staticRouting_s1->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s2 = staticRouting_s2->GetATPL4Protocol();

    // 这里得修改。还得改aggregator里的fanindegree
    // job1
    atpl4_s0->SetAggregatorFaninDegree(job1Id, 0b00000011);
    atpl4_s1->SetAggregatorFaninDegree(job1Id, 0b00001100);
    atpl4_s2->SetAggregatorFaninDegree(job1Id, 0b00001111);
    // job2
    atpl4_s0->SetAggregatorFaninDegree(job2Id, 0b00000011);
    atpl4_s1->SetAggregatorFaninDegree(job2Id, 0b00001100);
    atpl4_s2->SetAggregatorFaninDegree(job2Id, 0b00001111);

    // 配置w0的路由表 w0->s0->s2->ps1
    staticRouting_w0->AddHostRouteTo(ip_s2ps1.GetAddress(1), ip_w0s0.GetAddress(1), 1);

    // 配置w1的路由表 w1->s0->s2->ps1
    staticRouting_w1->AddHostRouteTo(ip_s2ps1.GetAddress(1), ip_w1s0.GetAddress(1), 1);

    // 配置w2的路由表 w2->s0->s2->ps2
    staticRouting_w2->AddHostRouteTo(ip_s2ps2.GetAddress(1), ip_w2s0.GetAddress(1), 1);

    // 配置w3的路由表 w3->s0->s2->ps2
    staticRouting_w3->AddHostRouteTo(ip_s2ps2.GetAddress(1), ip_w3s0.GetAddress(1), 1);

    // 配置w4的路由表 w4->s1->s2->ps1
    staticRouting_w4->AddHostRouteTo(ip_s2ps1.GetAddress(1), ip_w4s1.GetAddress(1), 1);

    // 配置w5的路由表 w5->s1->s2->ps1
    staticRouting_w5->AddHostRouteTo(ip_s2ps1.GetAddress(1), ip_w5s1.GetAddress(1), 1);

    // 配置w6的路由表 w6->s1->s2->ps2
    staticRouting_w6->AddHostRouteTo(ip_s2ps2.GetAddress(1), ip_w6s1.GetAddress(1), 1);

    // 配置w7的路由表 w7->s1->s2->ps2
    staticRouting_w7->AddHostRouteTo(ip_s2ps2.GetAddress(1), ip_w7s1.GetAddress(1), 1);

    // 配置s0的路由表 
    // s0->w0
    staticRouting_s0->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_w0s0.GetAddress(0), 1);
    // s0->w1
    staticRouting_s0->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_w1s0.GetAddress(0), 2);
    // s0->w2
    staticRouting_s0->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_w2s0.GetAddress(0), 3);
    // s0->w3
    staticRouting_s0->AddHostRouteTo(ip_w3s0.GetAddress(0), ip_w3s0.GetAddress(0), 4);
    // s0->s2->ps1
    staticRouting_s0->AddHostRouteTo(ip_s2ps1.GetAddress(1), ip_s0s2.GetAddress(1), 5);
    // s0->s2->ps2
    staticRouting_s0->AddHostRouteTo(ip_s2ps2.GetAddress(1), ip_s0s2.GetAddress(1), 5);

    // 配置s1的路由表
    // s1->w4
    staticRouting_s1->AddHostRouteTo(ip_w4s1.GetAddress(0), ip_w4s1.GetAddress(0), 1);
    // s1->w5
    staticRouting_s1->AddHostRouteTo(ip_w5s1.GetAddress(0), ip_w5s1.GetAddress(0), 2);
    // s1->w6
    staticRouting_s1->AddHostRouteTo(ip_w6s1.GetAddress(0), ip_w6s1.GetAddress(0), 3);
    // s1->w7
    staticRouting_s1->AddHostRouteTo(ip_w7s1.GetAddress(0), ip_w7s1.GetAddress(0), 4);
    // s1->s2->ps1
    staticRouting_s1->AddHostRouteTo(ip_s2ps1.GetAddress(1), ip_s1s2.GetAddress(1), 5);
    // s1->s2->ps2
    staticRouting_s1->AddHostRouteTo(ip_s2ps2.GetAddress(1), ip_s1s2.GetAddress(1), 5);

    // 配置s2的路由表
    // s2->s0->w0
    staticRouting_s2->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_s0s2.GetAddress(0), 1);
    // s2->s0->w1
    staticRouting_s2->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_s0s2.GetAddress(0), 1);
    // s2->s0->w2
    staticRouting_s2->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_s0s2.GetAddress(0), 1);
    // s2->s0->w3
    staticRouting_s2->AddHostRouteTo(ip_w3s0.GetAddress(0), ip_s0s2.GetAddress(0), 1);
    // s2->s1->w4
    staticRouting_s2->AddHostRouteTo(ip_w4s1.GetAddress(0), ip_s1s2.GetAddress(0), 2);
    // s2->s1->w5
    staticRouting_s2->AddHostRouteTo(ip_w5s1.GetAddress(0), ip_s1s2.GetAddress(0), 2);
    // s2->s1->w6
    staticRouting_s2->AddHostRouteTo(ip_w6s1.GetAddress(0), ip_s1s2.GetAddress(0), 2);
    // s2->s1->w7
    staticRouting_s2->AddHostRouteTo(ip_w7s1.GetAddress(0), ip_s1s2.GetAddress(0), 2);
    // s2->ps1
    staticRouting_s2->AddHostRouteTo(ip_s2ps1.GetAddress(1), ip_s2ps1.GetAddress(1), 3);
    // s2->ps2
    staticRouting_s2->AddHostRouteTo(ip_s2ps2.GetAddress(1), ip_s2ps2.GetAddress(1), 4);

    //配置ps1的路由表
    // ps1->s2->s0->w0
    staticRouting_ps1->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_s2ps1.GetAddress(0), 1);
    // ps1->s2->s0->w1
    staticRouting_ps1->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_s2ps1.GetAddress(0), 1);
    // ps1->s2->s1->w4
    staticRouting_ps1->AddHostRouteTo(ip_w4s1.GetAddress(0), ip_s2ps1.GetAddress(0), 1);
    // ps1->s2->s1->w5
    staticRouting_ps1->AddHostRouteTo(ip_w5s1.GetAddress(0), ip_s2ps1.GetAddress(0), 1);

    //陪着ps2的路由表
    // ps2->s2->s0->w2
    staticRouting_ps2->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_s2ps2.GetAddress(0), 1);
    // ps2->s2->s0->w3
    staticRouting_ps2->AddHostRouteTo(ip_w3s0.GetAddress(0), ip_s2ps2.GetAddress(0), 1);
    // ps2->s2->s1->w6
    staticRouting_ps2->AddHostRouteTo(ip_w6s1.GetAddress(0), ip_s2ps2.GetAddress(0), 1);
    // ps2->s2->s1->w7
    staticRouting_ps2->AddHostRouteTo(ip_w7s1.GetAddress(0), ip_s2ps2.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w0s0.GetAddress(0), sendPort1);  // job1 - w0
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w1s0.GetAddress(0), sendPort1);  // job1 - w1
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w4s1.GetAddress(0), sendPort1);  // job1 - w4
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w5s1.GetAddress(0), sendPort1);  // job1 - w5

    sinkATPSocket2->AddAddressMapping(job2Id, ip_w2s0.GetAddress(0), sendPort2);  // job2 - w2
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w3s0.GetAddress(0), sendPort2);  // job2 - w3
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w6s1.GetAddress(0), sendPort2);  // job2 - w6
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w7s1.GetAddress(0), sendPort2);  // job2 - w7
    

    // 开始测量
    Simulator::Schedule(Seconds(1.0), &MeasurementTxJob1, w0job1_ATPSocket);
    Simulator::Schedule(Seconds(1.0), &MeasurementTxJob2, w2job2_ATPSocket);

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    cwndStream_job1.close();
    cwndStream_job2.close();
    sendBytesStream_job1.close();
    sendBytesStream_job2.close();

    std::cout << "job1 Total Bytes Sent: " << w0job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job2 Total Bytes Sent: " << w2job2_ATPSocket->GetTotalTxBytes() << std::endl;

    return 0;
}
