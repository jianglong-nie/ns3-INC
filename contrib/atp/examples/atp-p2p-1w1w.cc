/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
// n0(job1) and m0(job2) to n2, n2 to n3
//
// - Flow from n0(job1) and m0(job2) to n3 using BulkSendApplication.
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

NS_LOG_COMPONENT_DEFINE("ATP-P2P-1w1w");

// 拥塞窗口跟踪，写入txt文件
std::ofstream cwndStream_n0_job1;
std::ofstream cwndStream_m0_job2;

// 添加队列长度跟踪文件流
std::ofstream queueSizeStream_n2;
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
    // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    
    // 在程序开始时打开文件流，使用trunc模式清空文件
    cwndStream_n0_job1.open("atp-result/trace-p2p-1w1w/n0-job1-cwnd-p2p-1w1w.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_m0_job2.open("atp-result/trace-p2p-1w1w/m0-job2-cwnd-p2p-1w1w.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-p2p-1w1w/job1-sinkBytes-p2p-1w1w.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-p2p-1w1w/job2-sinkBytes-p2p-1w1w.txt", std::ofstream::out | std::ofstream::trunc);
    queueSizeStream_n2.open("atp-result/trace-p2p-1w1w/n2-queueSize-p2p-1w1w.txt", std::ofstream::out | std::ofstream::trunc);
    

    uint32_t maxBytes = 248;
    Time stopTime = Seconds(1.0) + MicroSeconds(10000); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 400;

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_n0_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_m0_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(4); // 4个节点: n0,m0,n2,n3

    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2)); // n0-n2
    NodeContainer m0n2 = NodeContainer(nodes.Get(1), nodes.Get(2)); // m0-n2
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3)); // n2-n3

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));

    NetDeviceContainer d0d2, m0d2, d2d3;
    
    // Install n0 to n2 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    d0d2 = pointToPoint.Install(n0n2);

    // Install m0 to n2 links with 2us delay
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    m0d2 = pointToPoint.Install(m0n2);

    // Install n2 to n3 link
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));
    d2d3 = pointToPoint.Install(n2n3);

    
    // 设置n2上与n3连接部分的队列阈值
    Ptr<PointToPointNetDevice> n2Device = DynamicCast<PointToPointNetDevice>(d2d3.Get(0));
    NS_ASSERT(n2Device != nullptr); // 确保转换成功
    /*
    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p (大于100)

    // 直接在这个设备上设置自定义队列
    n2Device->SetQueue(customQueue);
    */

    n2Device->SetThreshold(80);
    n2Device->SetEnableEcn(true);
    
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

    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer im0i2 = ipv4Helper.Assign(m0d2);

    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4Helper.Assign(d2d3);

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
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    // start sinkApp
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(3)->AddApplication(sinkApp);

    //
    // Create sockets for job1 and job2
    //
    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> m0job2socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> n0job1_ATPSocket = DynamicCast<ATPSocket>(n0job1socket);
    Ptr<ATPSocket> m0job2_ATPSocket = DynamicCast<ATPSocket>(m0job2socket);

    // Create and configure applications
    Ptr<ATPBulkSendApplication> n0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> m0job2App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort = 11;  // Port for job1
    uint16_t sendPort2 = 12; // Different port for job2

    Address n0Address(InetSocketAddress(i0i2.GetAddress(0), sendPort));  // 使用端口11
    Address m0Address(InetSocketAddress(im0i2.GetAddress(0), sendPort2));  // 使用端口12

    // 设置job1初始拥塞窗口
    n0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    
    // Configure n0job1App
    n0job1App->Setup(sinkAddress, n0job1_ATPSocket, maxBytes, 1);
    n0job1App->SetEnableATPTag(true);
    n0job1App->SetStartTime(Seconds(1.0));
    n0job1App->SetStopTime(stopTime);
    n0job1App->SetFaninDegree(0b00000001); // 1个发送端
    n0job1App->SetWorkerId(0b00000001);

    n0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n0job1App));
    n0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n0job1App));
    n0job1_ATPSocket->Bind(n0Address);
    n0job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(0)->AddApplication(n0job1App);

    // 设置job2初始拥塞窗口
    m0job2_ATPSocket->SetInitCwnd(job2_initCwnd);

    // Configure m0job2App
    m0job2App->Setup(sinkAddress, m0job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    m0job2App->SetEnableATPTag(true);
    m0job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    m0job2App->SetStopTime(stopTime);
    m0job2App->SetFaninDegree(0b00000001); // 1个发送端
    m0job2App->SetWorkerId(0b00000001);

    m0job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, m0job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, m0job2App));
    m0job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, m0job2App));
    m0job2_ATPSocket->Bind(m0Address);
    m0job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(1)->AddApplication(m0job2App);

    // 连接拥塞窗口跟踪
    n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job1));
    m0job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_m0_job2));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_n0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_m0 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());

    staticRouting_n2->SetEnableAggregation(false); // 关闭聚合功能

    // 配置n0的路由表
    // n0到n3的路由:通过n2转发
    staticRouting_n0->AddHostRouteTo(i2i3.GetAddress(1), i0i2.GetAddress(1), 1);

    // 配置m0的路由表
    // m0到n3的路由:通过n2转发
    staticRouting_m0->AddHostRouteTo(i2i3.GetAddress(1), im0i2.GetAddress(1), 1);

    // 配置n2的路由表
    // n2到n0的路由
    staticRouting_n2->AddHostRouteTo(i0i2.GetAddress(0), i0i2.GetAddress(0), 1);
    // n2到m0的路由
    staticRouting_n2->AddHostRouteTo(im0i2.GetAddress(0), im0i2.GetAddress(0), 2);
    // n2到n3的路由
    staticRouting_n2->AddHostRouteTo(i2i3.GetAddress(1), i2i3.GetAddress(1), 3);

    // 配置n3的路由表
    // n3到n0的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(i0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到m0的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(im0i2.GetAddress(0), i2i3.GetAddress(0), 1);

    // 添加地址映射
    sinkATPSocket->AddAddressMapping(1, i0i2.GetAddress(0), sendPort);  // job1 - n0
    sinkATPSocket->AddAddressMapping(2, im0i2.GetAddress(0), sendPort2); // job2 - m0

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
    queueSizeStream_n2.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
