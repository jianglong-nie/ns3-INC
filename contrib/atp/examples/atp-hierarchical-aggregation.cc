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
    Simulator::Schedule(MicroSeconds(1000), &Measurement, sink);
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

    uint32_t maxBytes = 100;
    Time stopTime = Seconds(1.0) + MicroSeconds(1000); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    //uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;

    // 在文件打开后，写入初始拥塞窗口值
    //cwndStream_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(8); // 4个节点: w0,w1,w2,w3,s0,s1,s2,ps

    NodeContainer w0s0 = NodeContainer(nodes.Get(0), nodes.Get(4));
    NodeContainer w1s0 = NodeContainer(nodes.Get(1), nodes.Get(4));
    NodeContainer w2s1 = NodeContainer(nodes.Get(2), nodes.Get(5));
    NodeContainer w3s1 = NodeContainer(nodes.Get(3), nodes.Get(5));
    NodeContainer s0s2 = NodeContainer(nodes.Get(4), nodes.Get(6));
    NodeContainer s1s2 = NodeContainer(nodes.Get(5), nodes.Get(6));
    NodeContainer s2ps = NodeContainer(nodes.Get(6), nodes.Get(7));

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer dev_w0s0, dev_w1s0, dev_w2s1, dev_w3s1, dev_s0s2, dev_s1s2, dev_s2ps;
    dev_w0s0 = pointToPoint.Install(w0s0);
    dev_w1s0 = pointToPoint.Install(w1s0);
    dev_w2s1 = pointToPoint.Install(w2s1);
    dev_w3s1 = pointToPoint.Install(w3s1);
    dev_s0s2 = pointToPoint.Install(s0s2);
    dev_s1s2 = pointToPoint.Install(s1s2);
    dev_s2ps = pointToPoint.Install(s2ps);

    
    // 设置s0s2, s1s2, s2ps的队列阈值
    Ptr<PointToPointNetDevice> s0s2Device = DynamicCast<PointToPointNetDevice>(dev_s0s2.Get(0));
    Ptr<PointToPointNetDevice> s1s2Device = DynamicCast<PointToPointNetDevice>(dev_s1s2.Get(0));
    Ptr<PointToPointNetDevice> s2psDevice = DynamicCast<PointToPointNetDevice>(dev_s2ps.Get(0));
    NS_ASSERT(s0s2Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s2Device != nullptr); // 确保转换成功
    NS_ASSERT(s2psDevice != nullptr); // 确保转换成功
    /*
    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p (大于100)

    // 直接在这个设备上设置自定义队列
    n2Device->SetQueue(customQueue);
    */

    s0s2Device->SetThreshold(300);
    s1s2Device->SetThreshold(300);
    s2psDevice->SetThreshold(300);

    s0s2Device->SetEnableEcn(true);
    s1s2Device->SetEnableEcn(true);
    s2psDevice->SetEnableEcn(true);

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
    Ipv4InterfaceContainer ip_w2s1 = ipv4Helper.Assign(dev_w2s1);

    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w3s1 = ipv4Helper.Assign(dev_w3s1);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s2 = ipv4Helper.Assign(dev_s0s2);

    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s2 = ipv4Helper.Assign(dev_s1s2);

    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2ps = ipv4Helper.Assign(dev_s2ps);

    //
    // Create a PacketSinkApplication and install it on node 3
    //
    NS_LOG_INFO("Create Applications.");

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();

    // add address and port
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(ip_s2ps.GetAddress(1), sinkPort));
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    // create ATPSocket and bind to sinkAddress
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(7), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    // start sinkApp
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(7)->AddApplication(sinkApp);

    //
    // Create sockets for job
    //
    Ptr<Socket> w0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w2job1socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w3job1socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w0job1_ATPSocket = DynamicCast<ATPSocket>(w0job1socket);
    Ptr<ATPSocket> w1job1_ATPSocket = DynamicCast<ATPSocket>(w1job1socket);
    Ptr<ATPSocket> w2job1_ATPSocket = DynamicCast<ATPSocket>(w2job1socket);
    Ptr<ATPSocket> w3job1_ATPSocket = DynamicCast<ATPSocket>(w3job1socket);

    // Create and configure applications
    Ptr<ATPBulkSendApplication> w0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w2job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w3job1App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort = 11;  // Port for job1

    Address w0Address(InetSocketAddress(ip_w0s0.GetAddress(0), sendPort));  // 使用端口11
    Address w1Address(InetSocketAddress(ip_w1s0.GetAddress(0), sendPort));  // 使用端口11
    Address w2Address(InetSocketAddress(ip_w2s1.GetAddress(0), sendPort));  // 使用端口12
    Address w3Address(InetSocketAddress(ip_w3s1.GetAddress(0), sendPort));  // 使用端口12

    // 设置job1初始拥塞窗口
    w0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w1job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w2job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w3job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    
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
    w1job1App->SetFaninDegree(0b00001111); // 2个发送端
    w1job1App->SetWorkerId(0b00000010);

    w1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w1job1App));
    w1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w1job1App));
    w1job1_ATPSocket->Bind(w1Address);
    w1job1_ATPSocket->Connect(sinkAddress);
    nodes.Get(1)->AddApplication(w1job1App);

    //configure w2job1App
    w2job1App->Setup(sinkAddress, w2job1_ATPSocket, maxBytes, 1);
    w2job1App->SetEnableATPTag(true);
    w2job1App->SetStartTime(Seconds(1.0));
    w2job1App->SetStopTime(stopTime);
    w2job1App->SetFaninDegree(0b00001111); // 2个发送端
    w2job1App->SetWorkerId(0b00000100);

    w2job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w2job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w2job1App));
    w2job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w2job1App));
    w2job1_ATPSocket->Bind(w2Address);
    w2job1_ATPSocket->Connect(sinkAddress);
    nodes.Get(2)->AddApplication(w2job1App);

    //configure w3job1App
    w3job1App->Setup(sinkAddress, w3job1_ATPSocket, maxBytes, 1);
    w3job1App->SetEnableATPTag(true);
    w3job1App->SetStartTime(Seconds(1.0));
    w3job1App->SetStopTime(stopTime);
    w3job1App->SetFaninDegree(0b00001111); // 2个发送端
    w3job1App->SetWorkerId(0b00001000);

    w3job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w3job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w3job1App));
    w3job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w3job1App));
    w3job1_ATPSocket->Bind(w3Address);
    w3job1_ATPSocket->Connect(sinkAddress);
    nodes.Get(3)->AddApplication(w3job1App);

    // 连接拥塞窗口跟踪
    w0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_job1));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_w0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s0 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s1 = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s2 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_ps = staticRoutingHelper.GetStaticRouting(nodes.Get(7)->GetObject<Ipv4>());

    staticRouting_s0->SetEnableAggregation(true);
    staticRouting_s1->SetEnableAggregation(true);
    staticRouting_s2->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> atpl4_s0 = staticRouting_s0->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s1 = staticRouting_s1->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s2 = staticRouting_s2->GetATPL4Protocol();

    atpl4_s0->SetAggregatorFaninDegree(0b00000011);
    atpl4_s1->SetAggregatorFaninDegree(0b00001100);
    atpl4_s2->SetAggregatorFaninDegree(0b00001111);

    // 配置w0的路由表 w0->s0->s2->ps
    staticRouting_w0->AddHostRouteTo(ip_s2ps.GetAddress(1), ip_s0s2.GetAddress(0), 1);

    // 配置w1的路由表 w1->s0->s2->ps
    staticRouting_w1->AddHostRouteTo(ip_s2ps.GetAddress(1), ip_s0s2.GetAddress(0), 1);

    // 配置w2的路由表 w2->s1->s2->ps
    staticRouting_w2->AddHostRouteTo(ip_s2ps.GetAddress(1), ip_s1s2.GetAddress(0), 1);

    // 配置w3的路由表 w3->s1->s2->ps
    staticRouting_w3->AddHostRouteTo(ip_s2ps.GetAddress(1), ip_s1s2.GetAddress(0), 1);

    // 配置s0的路由表 
    // s0->w0
    staticRouting_s0->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_w0s0.GetAddress(0), 1);
    // s0->w1
    staticRouting_s0->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_w1s0.GetAddress(0), 2);
    // s0->s2->ps
    staticRouting_s0->AddHostRouteTo(ip_s2ps.GetAddress(1), ip_s0s2.GetAddress(1), 3);

    // 配置s1的路由表
    // s1->w2
    staticRouting_s1->AddHostRouteTo(ip_w2s1.GetAddress(0), ip_w2s1.GetAddress(0), 1);
    // s1->w3
    staticRouting_s1->AddHostRouteTo(ip_w3s1.GetAddress(0), ip_w3s1.GetAddress(0), 2);
    // s1->s2->ps
    staticRouting_s1->AddHostRouteTo(ip_s2ps.GetAddress(1), ip_s1s2.GetAddress(1), 3);

    // 配置s2的路由表
    // s2->s0->w0
    staticRouting_s2->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_s0s2.GetAddress(0), 1);
    // s2->s0->w1
    staticRouting_s2->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_s0s2.GetAddress(0), 1);
    // s2->s1->w2
    staticRouting_s2->AddHostRouteTo(ip_w2s1.GetAddress(0), ip_s1s2.GetAddress(0), 2);
    // s2->s1->w3
    staticRouting_s2->AddHostRouteTo(ip_w3s1.GetAddress(0), ip_s1s2.GetAddress(0), 2);
    // s2->ps
    staticRouting_s2->AddHostRouteTo(ip_s2ps.GetAddress(1), ip_s2ps.GetAddress(1), 3);

    //配置ps的路由表
    // ps->s0->w0
    staticRouting_ps->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_s2ps.GetAddress(0), 1);
    // ps->s0->w1
    staticRouting_ps->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_s2ps.GetAddress(0), 1);
    // ps->s1->w2
    staticRouting_ps->AddHostRouteTo(ip_w2s1.GetAddress(0), ip_s2ps.GetAddress(0), 1);
    // ps->s1->w3
    staticRouting_ps->AddHostRouteTo(ip_w3s1.GetAddress(0), ip_s2ps.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket->AddAddressMapping(1, ip_w0s0.GetAddress(0), sendPort);  // job1 - w0
    sinkATPSocket->AddAddressMapping(1, ip_w1s0.GetAddress(0), sendPort);  // job1 - w1
    sinkATPSocket->AddAddressMapping(1, ip_w2s1.GetAddress(0), sendPort);  // job1 - w2
    sinkATPSocket->AddAddressMapping(1, ip_w3s1.GetAddress(0), sendPort);  // job1 - w3
    
    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
