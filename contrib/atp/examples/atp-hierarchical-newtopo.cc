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

// 每个worker独立的发送字节数记录
std::ofstream sendBytesStream_workers[10];  // w0-w9
uint64_t lastTimeWorkerBytes[10] = {0};     // 初始化为0

// 通用的发送字节数测量函数
static void MeasurementTxWorker(Ptr<ATPSocket> socket, std::ofstream& stream, uint64_t& lastBytes)
{
    Time now = Simulator::Now();
    uint64_t currentBytes = socket->GetTotalTxBytes();
    uint64_t bytesPerInterval = currentBytes - lastBytes;
    
    stream << now.GetMicroSeconds() << "\t" << currentBytes << "\t" << bytesPerInterval << std::endl;
    
    lastBytes = currentBytes;
    
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &MeasurementTxWorker, socket, std::ref(stream), std::ref(lastBytes));
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
    // LogComponentEnable("ATPSocket", LOG_LEVEL_INFO);
    // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    
    // 注意：超时重传机制已启用
    // 默认超时时间：500微秒，检查间隔：100微秒
    // 可通过下方参数调整

    cwndStream_job1.open("atp-result/trace-ha-newtopo/job1-cwnd-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_job2.open("atp-result/trace-ha-newtopo/job2-cwnd-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    
    sendBytesStream_workers[0].open("atp-result/trace-ha-newtopo/job1-sendBytes-w0.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[1].open("atp-result/trace-ha-newtopo/job1-sendBytes-w1.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[2].open("atp-result/trace-ha-newtopo/job1-sendBytes-w2.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[3].open("atp-result/trace-ha-newtopo/job1-sendBytes-w3.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[4].open("atp-result/trace-ha-newtopo/job1-sendBytes-w4.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[5].open("atp-result/trace-ha-newtopo/job2-sendBytes-w5.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[6].open("atp-result/trace-ha-newtopo/job2-sendBytes-w6.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[7].open("atp-result/trace-ha-newtopo/job2-sendBytes-w7.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[8].open("atp-result/trace-ha-newtopo/job2-sendBytes-w8.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_workers[9].open("atp-result/trace-ha-newtopo/job2-sendBytes-w9.txt", std::ofstream::out | std::ofstream::trunc);
    uint32_t maxBytes = 0;
    Time stopTime = Seconds(1.0) + MicroSeconds(10001); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;
    
    // 设置超时重传参数
    // 当数据包发送后超过retxTimeout时间未收到ACK，将触发重传
    // retxCheckInterval是定期检查超时的间隔时间
    Time retxTimeout = MicroSeconds(20);         // 重传超时时间：16us（可调整）
    Time retxCheckInterval = MicroSeconds(2);   // 超时检查间隔：2us（可调整）

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(16); 
    // 节点: w0,w1,w2,w3,w4,w5,w6,w7,w8,w9, s0,s1,s2,s3,ps1,ps2
    // job1: w0, w1, w2, w3, w4
    // job2: w5, w6, w7, w8, w9

    NodeContainer w0s0 = NodeContainer(nodes.Get(0), nodes.Get(10));
    NodeContainer w1s0 = NodeContainer(nodes.Get(1), nodes.Get(10));
    NodeContainer w2s0 = NodeContainer(nodes.Get(2), nodes.Get(10));

    NodeContainer w3s1 = NodeContainer(nodes.Get(3), nodes.Get(11));
    NodeContainer w4s1 = NodeContainer(nodes.Get(4), nodes.Get(11));
    NodeContainer w5s1 = NodeContainer(nodes.Get(5), nodes.Get(11));
    NodeContainer w6s1 = NodeContainer(nodes.Get(6), nodes.Get(11));

    NodeContainer w7s2 = NodeContainer(nodes.Get(7), nodes.Get(12));
    NodeContainer w8s2 = NodeContainer(nodes.Get(8), nodes.Get(12));
    NodeContainer w9s2 = NodeContainer(nodes.Get(9), nodes.Get(12));

    NodeContainer s0s3 = NodeContainer(nodes.Get(10), nodes.Get(13));
    NodeContainer s1s3 = NodeContainer(nodes.Get(11), nodes.Get(13));
    NodeContainer s2s3 = NodeContainer(nodes.Get(12), nodes.Get(13));

    NodeContainer s3ps1 = NodeContainer(nodes.Get(13), nodes.Get(14));
    NodeContainer s3ps2 = NodeContainer(nodes.Get(13), nodes.Get(15));

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1us"));

    NetDeviceContainer dev_w0s0, dev_w1s0, dev_w2s0;
    NetDeviceContainer dev_w3s1, dev_w4s1, dev_w5s1, dev_w6s1;
    NetDeviceContainer dev_w7s2, dev_w8s2, dev_w9s2;

    NetDeviceContainer dev_s0s3, dev_s1s3, dev_s2s3;
    NetDeviceContainer dev_s3ps1, dev_s3ps2;

    dev_w0s0 = pointToPoint.Install(w0s0);
    dev_w1s0 = pointToPoint.Install(w1s0);
    dev_w2s0 = pointToPoint.Install(w2s0);

    dev_w3s1 = pointToPoint.Install(w3s1);
    dev_w4s1 = pointToPoint.Install(w4s1);
    dev_w5s1 = pointToPoint.Install(w5s1);
    dev_w6s1 = pointToPoint.Install(w6s1);

    dev_w7s2 = pointToPoint.Install(w7s2);
    dev_w8s2 = pointToPoint.Install(w8s2);
    dev_w9s2 = pointToPoint.Install(w9s2);

    dev_s0s3 = pointToPoint.Install(s0s3);
    dev_s1s3 = pointToPoint.Install(s1s3);
    dev_s2s3 = pointToPoint.Install(s2s3);

    dev_s3ps1 = pointToPoint.Install(s3ps1);
    dev_s3ps2 = pointToPoint.Install(s3ps2);

    
    // 设置s0s2, s1s2, s2ps的队列阈值
    Ptr<PointToPointNetDevice> s0s3Device = DynamicCast<PointToPointNetDevice>(dev_s0s3.Get(0));
    Ptr<PointToPointNetDevice> s1s3Device = DynamicCast<PointToPointNetDevice>(dev_s1s3.Get(0));
    Ptr<PointToPointNetDevice> s2s3Device = DynamicCast<PointToPointNetDevice>(dev_s2s3.Get(0));
    Ptr<PointToPointNetDevice> s3ps1Device = DynamicCast<PointToPointNetDevice>(dev_s3ps1.Get(0));
    Ptr<PointToPointNetDevice> s3ps2Device = DynamicCast<PointToPointNetDevice>(dev_s3ps2.Get(0));
    NS_ASSERT(s0s3Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s3Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s3Device != nullptr); // 确保转换成功
    NS_ASSERT(s3ps1Device != nullptr); // 确保转换成功
    NS_ASSERT(s3ps2Device != nullptr); // 确保转换成功
    /*
    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p (大于100)

    // 直接在这个设备上设置自定义队列
    n2Device->SetQueue(customQueue);
    */

    s0s3Device->SetThreshold(350);
    s1s3Device->SetThreshold(350);
    s2s3Device->SetThreshold(350);
    s3ps1Device->SetThreshold(350);
    s3ps2Device->SetThreshold(350);

    s0s3Device->SetEnableEcn(true);
    s1s3Device->SetEnableEcn(true);
    s2s3Device->SetEnableEcn(true);
    s3ps1Device->SetEnableEcn(true);
    s3ps2Device->SetEnableEcn(true);

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
    Ipv4InterfaceContainer ip_w3s1 = ipv4Helper.Assign(dev_w3s1);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w4s1 = ipv4Helper.Assign(dev_w4s1);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w5s1 = ipv4Helper.Assign(dev_w5s1);

    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w6s1 = ipv4Helper.Assign(dev_w6s1);

    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w7s2 = ipv4Helper.Assign(dev_w7s2);

    ipv4Helper.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w8s2 = ipv4Helper.Assign(dev_w8s2);

    ipv4Helper.SetBase("10.1.10.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w9s2 = ipv4Helper.Assign(dev_w9s2);

    ipv4Helper.SetBase("10.1.11.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s3 = ipv4Helper.Assign(dev_s0s3);

    ipv4Helper.SetBase("10.1.12.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s3 = ipv4Helper.Assign(dev_s1s3);

    ipv4Helper.SetBase("10.1.13.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s3 = ipv4Helper.Assign(dev_s2s3);

    ipv4Helper.SetBase("10.1.14.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3ps1 = ipv4Helper.Assign(dev_s3ps1);

    ipv4Helper.SetBase("10.1.15.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3ps2 = ipv4Helper.Assign(dev_s3ps2);

    //
    // Create a PacketSinkApplication and install it on node 3
    //
    NS_LOG_INFO("Create Applications.");

    // Create sink applications for both jobs
    Ptr<ATPPacketSink> sinkApp1 = CreateObject<ATPPacketSink>();
    Ptr<ATPPacketSink> sinkApp2 = CreateObject<ATPPacketSink>();

    // job1 sink (ps1)
    uint16_t sinkPort1 = 9;
    Address sinkAddress1(InetSocketAddress(ip_s3ps1.GetAddress(1), sinkPort1));
    sinkApp1->SetAddressPort(sinkAddress1, sinkPort1);

    // job2 sink (ps2)
    uint16_t sinkPort2 = 9;
    Address sinkAddress2(InetSocketAddress(ip_s3ps2.GetAddress(1), sinkPort2));
    sinkApp2->SetAddressPort(sinkAddress2, sinkPort2);

    // create ATPSocket for sink1 and bind to sinkAddress1
    Ptr<Socket> sinkSocket1 = Socket::CreateSocket(nodes.Get(14), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket1 = DynamicCast<ATPSocket>(sinkSocket1);
    sinkApp1->SetSocket(sinkATPSocket1);
    sinkATPSocket1->Bind(sinkAddress1);
    sinkATPSocket1->Listen();

    // create ATPSocket for sink2 and bind to sinkAddress2
    Ptr<Socket> sinkSocket2 = Socket::CreateSocket(nodes.Get(15), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket2 = DynamicCast<ATPSocket>(sinkSocket2);
    sinkApp2->SetSocket(sinkATPSocket2);
    sinkATPSocket2->Bind(sinkAddress2);
    sinkATPSocket2->Listen();

    // start sink applications
    sinkApp1->SetStartTime(Seconds(0.0));
    sinkApp1->SetStopTime(stopTime);
    nodes.Get(14)->AddApplication(sinkApp1);  // ps1

    sinkApp2->SetStartTime(Seconds(0.0));
    sinkApp2->SetStopTime(stopTime);
    nodes.Get(15)->AddApplication(sinkApp2);  // ps2

    //
    // Create sockets for job1 (w0, w1, w2, w3, w4)
    //
    Ptr<Socket> w0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w2job1socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w3job1socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w4job1socket = Socket::CreateSocket(nodes.Get(4), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w0job1_ATPSocket = DynamicCast<ATPSocket>(w0job1socket);
    Ptr<ATPSocket> w1job1_ATPSocket = DynamicCast<ATPSocket>(w1job1socket);
    Ptr<ATPSocket> w2job1_ATPSocket = DynamicCast<ATPSocket>(w2job1socket);
    Ptr<ATPSocket> w3job1_ATPSocket = DynamicCast<ATPSocket>(w3job1socket);
    Ptr<ATPSocket> w4job1_ATPSocket = DynamicCast<ATPSocket>(w4job1socket);
    //
    // Create sockets for job2 (w5, w6, w7, w8, w9)
    //
    Ptr<Socket> w5job2socket = Socket::CreateSocket(nodes.Get(5), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w6job2socket = Socket::CreateSocket(nodes.Get(6), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w7job2socket = Socket::CreateSocket(nodes.Get(7), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w8job2socket = Socket::CreateSocket(nodes.Get(8), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w9job2socket = Socket::CreateSocket(nodes.Get(9), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w5job2_ATPSocket = DynamicCast<ATPSocket>(w5job2socket);
    Ptr<ATPSocket> w6job2_ATPSocket = DynamicCast<ATPSocket>(w6job2socket);
    Ptr<ATPSocket> w7job2_ATPSocket = DynamicCast<ATPSocket>(w7job2socket);
    Ptr<ATPSocket> w8job2_ATPSocket = DynamicCast<ATPSocket>(w8job2socket);
    Ptr<ATPSocket> w9job2_ATPSocket = DynamicCast<ATPSocket>(w9job2socket);

    // Create applications for job1
    Ptr<ATPBulkSendApplication> w0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w2job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w3job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w4job1App = CreateObject<ATPBulkSendApplication>();

    // Create applications for job2
    Ptr<ATPBulkSendApplication> w5job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w6job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w7job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w8job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w9job2App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort1 = 11;  // Port for job1
    uint16_t sendPort2 = 12;  // Port for job2

    // Addresses for job1 workers
    Address w0Address(InetSocketAddress(ip_w0s0.GetAddress(0), sendPort1));
    Address w1Address(InetSocketAddress(ip_w1s0.GetAddress(0), sendPort1));
    Address w2Address(InetSocketAddress(ip_w2s0.GetAddress(0), sendPort1));
    Address w3Address(InetSocketAddress(ip_w3s1.GetAddress(0), sendPort1));
    Address w4Address(InetSocketAddress(ip_w4s1.GetAddress(0), sendPort1));

    // Addresses for job2 workers
    Address w5Address(InetSocketAddress(ip_w5s1.GetAddress(0), sendPort2));
    Address w6Address(InetSocketAddress(ip_w6s1.GetAddress(0), sendPort2));
    Address w7Address(InetSocketAddress(ip_w7s2.GetAddress(0), sendPort2));
    Address w8Address(InetSocketAddress(ip_w8s2.GetAddress(0), sendPort2));
    Address w9Address(InetSocketAddress(ip_w9s2.GetAddress(0), sendPort2)); 

    // 设置job1初始拥塞窗口和超时参数
    w0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w0job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w0job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w1job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w1job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w1job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w2job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w2job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w2job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w3job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w3job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w3job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w4job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w4job1_ATPSocket->SetRetxTimeout(retxTimeout);
    w4job1_ATPSocket->SetRetxCheckInterval(retxCheckInterval);

    // 设置job2初始拥塞窗口和超时参数
    w5job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w5job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w5job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w6job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w6job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w6job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);

    w7job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w7job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w7job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w8job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w8job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w8job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    w9job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w9job2_ATPSocket->SetRetxTimeout(retxTimeout);
    w9job2_ATPSocket->SetRetxCheckInterval(retxCheckInterval);
    
    // Configure w0job1App
    uint8_t job1Id = 1;
    w0job1App->Setup(sinkAddress1, w0job1_ATPSocket, maxBytes, job1Id);
    w0job1App->SetEnableATPTag(true);
    w0job1App->SetStartTime(Seconds(1.0));
    w0job1App->SetStopTime(stopTime);
    w0job1App->SetFaninDegree(0b00011111); // 5个发送端 job1
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
    w1job1App->SetFaninDegree(0b00011111); // 5个发送端 job1
    w1job1App->SetWorkerId(0b00000010);

    w1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w1job1App));
    w1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w1job1App));
    w1job1_ATPSocket->Bind(w1Address);
    w1job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(1)->AddApplication(w1job1App);

    // Configure w2job1App
    w2job1App->Setup(sinkAddress1, w2job1_ATPSocket, maxBytes, job1Id);
    w2job1App->SetEnableATPTag(true);
    w2job1App->SetStartTime(Seconds(1.0));
    w2job1App->SetStopTime(stopTime);
    w2job1App->SetFaninDegree(0b00011111); // 5个发送端 job1
    w2job1App->SetWorkerId(0b00000100);

    w2job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w2job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w2job1App));
    w2job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w2job1App));
    w2job1_ATPSocket->Bind(w2Address);
    w2job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(2)->AddApplication(w2job1App);

    // Configure w3job1App
    w3job1App->Setup(sinkAddress1, w3job1_ATPSocket, maxBytes, job1Id);
    w3job1App->SetEnableATPTag(true);
    w3job1App->SetStartTime(Seconds(1.0));
    w3job1App->SetStopTime(stopTime);
    w3job1App->SetFaninDegree(0b00011111);  // 5个发送端 job1
    w3job1App->SetWorkerId(0b00001000);

    w3job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w3job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w3job1App));
    w3job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w3job1App));
    w3job1_ATPSocket->Bind(w3Address);
    w3job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(3)->AddApplication(w3job1App);

    // Configure w4job1App
    w4job1App->Setup(sinkAddress1, w4job1_ATPSocket, maxBytes, job1Id);
    w4job1App->SetEnableATPTag(true);
    w4job1App->SetStartTime(Seconds(1.0));
    w4job1App->SetStopTime(stopTime);
    w4job1App->SetFaninDegree(0b00011111); // 5个发送端 job1
    w4job1App->SetWorkerId(0b00010000);
    
    w4job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w4job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w4job1App));
    w4job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w4job1App));
    w4job1_ATPSocket->Bind(w4Address);
    w4job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(4)->AddApplication(w4job1App);

    // Configure w5job2App
    uint8_t job2Id = 2;
    w5job2App->Setup(sinkAddress2, w5job2_ATPSocket, maxBytes, job2Id);
    w5job2App->SetEnableATPTag(true);
    w5job2App->SetStartTime(Seconds(1.0));
    w5job2App->SetStopTime(stopTime);
    w5job2App->SetFaninDegree(0b00011111); // 5个发送端 job2
    w5job2App->SetWorkerId(0b00000001);

    w5job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w5job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w5job2App));
    w5job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w5job2App));
    w5job2_ATPSocket->Bind(w5Address);
    w5job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(5)->AddApplication(w5job2App);

    // Configure w6job2App
    w6job2App->Setup(sinkAddress2, w6job2_ATPSocket, maxBytes, job2Id);
    w6job2App->SetEnableATPTag(true);
    w6job2App->SetStartTime(Seconds(1.0));
    w6job2App->SetStopTime(stopTime);
    w6job2App->SetFaninDegree(0b00011111); // 5个发送端 job2
    w6job2App->SetWorkerId(0b00000010);

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
    w7job2App->SetFaninDegree(0b00011111); // 5个发送端 job2
    w7job2App->SetWorkerId(0b00000100);

    w7job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w7job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w7job2App));
    w7job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w7job2App));
    w7job2_ATPSocket->Bind(w7Address);
    w7job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(7)->AddApplication(w7job2App);

    // Configure w8job2App
    w8job2App->Setup(sinkAddress2, w8job2_ATPSocket, maxBytes, job2Id);
    w8job2App->SetEnableATPTag(true);
    w8job2App->SetStartTime(Seconds(1.0));
    w8job2App->SetStopTime(stopTime);
    w8job2App->SetFaninDegree(0b00011111); // 5个发送端 job2
    w8job2App->SetWorkerId(0b00001000);

    w8job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w8job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w8job2App));
    w8job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w8job2App));
    w8job2_ATPSocket->Bind(w8Address);
    w8job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(8)->AddApplication(w8job2App);

    // Configure w9job2App
    w9job2App->Setup(sinkAddress2, w9job2_ATPSocket, maxBytes, job2Id);
    w9job2App->SetEnableATPTag(true);
    w9job2App->SetStartTime(Seconds(1.0));
    w9job2App->SetStopTime(stopTime);
    w9job2App->SetFaninDegree(0b00011111); // 5个发送端 job2
    w9job2App->SetWorkerId(0b00010000);

    w9job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w9job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w9job2App));
    w9job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w9job2App));
    w9job2_ATPSocket->Bind(w9Address);
    w9job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(9)->AddApplication(w9job2App);

    // 连接拥塞窗口跟踪
    w0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_job1));
    w9job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
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
    Ptr<ATPStaticRouting> staticRouting_w8 = staticRoutingHelper.GetStaticRouting(nodes.Get(8)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w9 = staticRoutingHelper.GetStaticRouting(nodes.Get(9)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s0 = staticRoutingHelper.GetStaticRouting(nodes.Get(10)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s1 = staticRoutingHelper.GetStaticRouting(nodes.Get(11)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s2 = staticRoutingHelper.GetStaticRouting(nodes.Get(12)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s3 = staticRoutingHelper.GetStaticRouting(nodes.Get(13)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps1 = staticRoutingHelper.GetStaticRouting(nodes.Get(14)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps2 = staticRoutingHelper.GetStaticRouting(nodes.Get(15)->GetObject<Ipv4>());
    
    staticRouting_s0->SetEnableAggregation(true);
    staticRouting_s1->SetEnableAggregation(true);
    staticRouting_s2->SetEnableAggregation(true);
    staticRouting_s3->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> atpl4_s0 = staticRouting_s0->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s1 = staticRouting_s1->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s2 = staticRouting_s2->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s3 = staticRouting_s3->GetATPL4Protocol();

    // 设置层ID：s0, s1, s2是第一层聚合器，s3是第二层聚合器
    // 不同层使用不同的哈希函数，避免连环冲突
    atpl4_s0->SetLayerId(1);  // 第一层聚合
    atpl4_s1->SetLayerId(1);  // 第一层聚合
    atpl4_s2->SetLayerId(1);  // 第一层聚合
    atpl4_s3->SetLayerId(2);  // 第二层聚合

    // 这里得修改。还得改aggregator里的fanindegree
    // job1
    atpl4_s0->SetAggregatorFaninDegree(job1Id, 0b00000111);
    atpl4_s1->SetAggregatorFaninDegree(job1Id, 0b00011000);
    atpl4_s3->SetAggregatorFaninDegree(job1Id, 0b00011111);
    // job2
    atpl4_s1->SetAggregatorFaninDegree(job2Id, 0b00000011);
    atpl4_s2->SetAggregatorFaninDegree(job2Id, 0b00011100);
    atpl4_s3->SetAggregatorFaninDegree(job2Id, 0b00011111);

    // 配置w0的路由表 w0->s0->s3->ps1
    staticRouting_w0->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_w0s0.GetAddress(1), 1);

    // 配置w1的路由表 w1->s0->s3->ps1
    staticRouting_w1->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_w1s0.GetAddress(1), 1);

    // 配置w2的路由表 w2->s0->s3->ps1
    staticRouting_w2->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_w2s0.GetAddress(1), 1);

    // 配置w3的路由表 w3->s1->s3->ps1
    staticRouting_w3->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_w3s1.GetAddress(1), 1);

    // 配置w4的路由表 w4->s1->s3->ps1
    staticRouting_w4->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_w4s1.GetAddress(1), 1);

    // 配置w5的路由表 w5->s1->s3->ps2
    staticRouting_w5->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_w5s1.GetAddress(1), 1);

    // 配置w6的路由表 w6->s1->s3->ps2
    staticRouting_w6->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_w6s1.GetAddress(1), 1);

    // 配置w7的路由表 w7->s2->s3->ps2
    staticRouting_w7->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_w7s2.GetAddress(1), 1);
    
    // 配置w8的路由表 w8->s2->s3->ps2
    staticRouting_w8->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_w8s2.GetAddress(1), 1);

    // 配置w9的路由表 w9->s2->s3->ps2
    staticRouting_w9->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_w9s2.GetAddress(1), 1);

    // 配置s0的路由表 
    // s0->w0
    staticRouting_s0->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_w0s0.GetAddress(0), 1);
    // s0->w1
    staticRouting_s0->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_w1s0.GetAddress(0), 2);
    // s0->w2
    staticRouting_s0->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_w2s0.GetAddress(0), 3);
    // s0->s3->ps1
    staticRouting_s0->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_s0s3.GetAddress(1), 4);


    // 配置s1的路由表
    // s1->w3
    staticRouting_s1->AddHostRouteTo(ip_w3s1.GetAddress(0), ip_w3s1.GetAddress(0), 1);
    // s1->w4
    staticRouting_s1->AddHostRouteTo(ip_w4s1.GetAddress(0), ip_w4s1.GetAddress(0), 2);
    // s1->w5
    staticRouting_s1->AddHostRouteTo(ip_w5s1.GetAddress(0), ip_w5s1.GetAddress(0), 3);
    // s1->w6
    staticRouting_s1->AddHostRouteTo(ip_w6s1.GetAddress(0), ip_w6s1.GetAddress(0), 4);
    // s1->s3->ps1
    staticRouting_s1->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_s1s3.GetAddress(1), 5);
    // s1->s3->ps2
    staticRouting_s1->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_s1s3.GetAddress(1), 5);

    // 配置s2的路由表
    // s2->w7
    staticRouting_s2->AddHostRouteTo(ip_w7s2.GetAddress(0), ip_w7s2.GetAddress(0), 1);
    // s2->w8
    staticRouting_s2->AddHostRouteTo(ip_w8s2.GetAddress(0), ip_w8s2.GetAddress(0), 2);
    // s2->w9
    staticRouting_s2->AddHostRouteTo(ip_w9s2.GetAddress(0), ip_w9s2.GetAddress(0), 3);
    // s2->s3->ps2
    staticRouting_s2->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_s2s3.GetAddress(1), 4);

    // 配置s3的路由表
    // s3->s0->w0
    staticRouting_s3->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_s0s3.GetAddress(0), 1);
    // s3->s0->w1
    staticRouting_s3->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_s0s3.GetAddress(0), 1);
    // s3->s0->w2
    staticRouting_s3->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_s0s3.GetAddress(0), 1);
    // s3->s1->w3
    staticRouting_s3->AddHostRouteTo(ip_w3s1.GetAddress(0), ip_s1s3.GetAddress(0), 2);
    // s3->s1->w4
    staticRouting_s3->AddHostRouteTo(ip_w4s1.GetAddress(0), ip_s1s3.GetAddress(0), 2);
    // s3->s1->w5
    staticRouting_s3->AddHostRouteTo(ip_w5s1.GetAddress(0), ip_s1s3.GetAddress(0), 2);
    // s3->s1->w6
    staticRouting_s3->AddHostRouteTo(ip_w6s1.GetAddress(0), ip_s1s3.GetAddress(0), 2);
    // s3->s2->w7
    staticRouting_s3->AddHostRouteTo(ip_w7s2.GetAddress(0), ip_s2s3.GetAddress(0), 3);
    // s3->s2->w8
    staticRouting_s3->AddHostRouteTo(ip_w8s2.GetAddress(0), ip_s2s3.GetAddress(0), 3);
    // s3->s2->w9
    staticRouting_s3->AddHostRouteTo(ip_w9s2.GetAddress(0), ip_s2s3.GetAddress(0), 3);
    // s3->ps1
    staticRouting_s3->AddHostRouteTo(ip_s3ps1.GetAddress(1), ip_s3ps1.GetAddress(1), 4);
    // s3->ps2
    staticRouting_s3->AddHostRouteTo(ip_s3ps2.GetAddress(1), ip_s3ps2.GetAddress(1), 5);


    //配置ps1的路由表
    // ps1->s3->s0->w0
    staticRouting_ps1->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_s3ps1.GetAddress(0), 1);
    // ps1->s3->s0->w1
    staticRouting_ps1->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_s3ps1.GetAddress(0), 1);
    // ps1->s3->s0->w2
    staticRouting_ps1->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_s3ps1.GetAddress(0), 1);
    // ps1->s3->s1->w3
    staticRouting_ps1->AddHostRouteTo(ip_w3s1.GetAddress(0), ip_s3ps1.GetAddress(0), 1);
    // ps1->s3->s1->w4
    staticRouting_ps1->AddHostRouteTo(ip_w4s1.GetAddress(0), ip_s3ps1.GetAddress(0), 1);

    //配置ps2的路由表
    // ps2->s3->s1->w5
    staticRouting_ps2->AddHostRouteTo(ip_w5s1.GetAddress(0), ip_s3ps2.GetAddress(0), 1);
    // ps2->s3->s1->w6
    staticRouting_ps2->AddHostRouteTo(ip_w6s1.GetAddress(0), ip_s3ps2.GetAddress(0), 1);
    // ps2->s3->s2->w7
    staticRouting_ps2->AddHostRouteTo(ip_w7s2.GetAddress(0), ip_s3ps2.GetAddress(0), 1);
    // ps2->s3->s2->w8
    staticRouting_ps2->AddHostRouteTo(ip_w8s2.GetAddress(0), ip_s3ps2.GetAddress(0), 1);
    // ps2->s3->s2->w9
    staticRouting_ps2->AddHostRouteTo(ip_w9s2.GetAddress(0), ip_s3ps2.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w0s0.GetAddress(0), sendPort1);  // job1 - w0
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w1s0.GetAddress(0), sendPort1);  // job1 - w1
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w2s0.GetAddress(0), sendPort1);  // job1 - w2
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w3s1.GetAddress(0), sendPort1);  // job1 - w3
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w4s1.GetAddress(0), sendPort1);  // job1 - w4

    sinkATPSocket2->AddAddressMapping(job2Id, ip_w5s1.GetAddress(0), sendPort2);  // job2 - w5
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w6s1.GetAddress(0), sendPort2);  // job2 - w6
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w7s2.GetAddress(0), sendPort2);  // job2 - w7
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w8s2.GetAddress(0), sendPort2);  // job2 - w8
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w9s2.GetAddress(0), sendPort2);  // job2 - w9

    // 开始测量 - 为每个worker调度测量
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w0job1_ATPSocket, std::ref(sendBytesStream_workers[0]), std::ref(lastTimeWorkerBytes[0]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w1job1_ATPSocket, std::ref(sendBytesStream_workers[1]), std::ref(lastTimeWorkerBytes[1]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w2job1_ATPSocket, std::ref(sendBytesStream_workers[2]), std::ref(lastTimeWorkerBytes[2]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w3job1_ATPSocket, std::ref(sendBytesStream_workers[3]), std::ref(lastTimeWorkerBytes[3]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w4job1_ATPSocket, std::ref(sendBytesStream_workers[4]), std::ref(lastTimeWorkerBytes[4]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w5job2_ATPSocket, std::ref(sendBytesStream_workers[5]), std::ref(lastTimeWorkerBytes[5]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w6job2_ATPSocket, std::ref(sendBytesStream_workers[6]), std::ref(lastTimeWorkerBytes[6]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w7job2_ATPSocket, std::ref(sendBytesStream_workers[7]), std::ref(lastTimeWorkerBytes[7]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w8job2_ATPSocket, std::ref(sendBytesStream_workers[8]), std::ref(lastTimeWorkerBytes[8]));
    Simulator::Schedule(Seconds(1.0), &MeasurementTxWorker, w9job2_ATPSocket, std::ref(sendBytesStream_workers[9]), std::ref(lastTimeWorkerBytes[9]));
        
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
    for (int i = 0; i < 10; i++) {
        sendBytesStream_workers[i].close();
    }


    std::cout << "-------------job1-------------------" << std::endl;
    for (int i = 0; i < 5; i++) {
        std::cout << "worker " << i << " total bytes sent: " << lastTimeWorkerBytes[i] << std::endl;
    }
    
    std::cout << "-------------job2-------------------" << std::endl;
    for (int i = 5; i < 10; i++) {
        std::cout << "worker " << i << " total bytes sent: " << lastTimeWorkerBytes[i] << std::endl;
    }

    std::cout << "--------------------------------" << std::endl;
    std::cout << " job1-PS total recv bytes: " << sinkApp1->GetTotalRxJob(job1Id) << std::endl;
    std::cout << " job2-PS total recv bytes: " << sinkApp2->GetTotalRxJob(job2Id) << std::endl;

    // Get atp l4 protocol
    Ptr<ATPL4Protocol> s0_atpl4 = staticRouting_s0->GetATPL4Protocol();
    Ptr<ATPL4Protocol> s1_atpl4 = staticRouting_s1->GetATPL4Protocol();
    Ptr<ATPL4Protocol> s2_atpl4 = staticRouting_s2->GetATPL4Protocol();
    Ptr<ATPL4Protocol> s3_atpl4 = staticRouting_s3->GetATPL4Protocol();

    // 输出哈希冲突次数
    std::cout << "s0_atpl4 job1 hash collision counter: " << s0_atpl4->GetJobIdHashCollisionCounter(job1Id) << std::endl;
    
    std::cout << "s1_atpl4 job1 hash collision counter: " << s1_atpl4->GetJobIdHashCollisionCounter(job1Id) << std::endl;
    std::cout << "s1_atpl4 job2 hash collision counter: " << s1_atpl4->GetJobIdHashCollisionCounter(job2Id) << std::endl;
    
    std::cout << "s2_atpl4 job2 hash collision counter: " << s2_atpl4->GetJobIdHashCollisionCounter(job2Id) << std::endl;
    
    std::cout << "s3_atpl4 job1 hash collision counter: " << s3_atpl4->GetJobIdHashCollisionCounter(job1Id) << std::endl;
    std::cout << "s3_atpl4 job2 hash collision counter: " << s3_atpl4->GetJobIdHashCollisionCounter(job2Id) << std::endl;


    return 0;
}
