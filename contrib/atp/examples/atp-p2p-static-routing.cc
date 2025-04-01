/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
//
//       n0 ----------- n1
//            500 Kbps
//             5 ms
//
//        n0                
//         \ 500 Kbps                   
//          \ 5 ms        500 Kbps 5ms          
//           n2--------------------------n3             
//          / 500 Kbps                   
//         / 5 ms                   
//        n1                     
//
//
// - Flow from n0 to n1 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "tcp-bulk-send.tr"
//   and pcap tracing available when tracing is turned on.

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/atp-module.h" // atp module
#include "ns3/ipv4-static-routing-helper.h"

#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATP-P2P");

// 拥塞窗口跟踪，写入txt文件
std::ofstream cwndStream_n0_job1;
std::ofstream cwndStream_n1_job1;

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

std::ofstream SinkBytesStream_job1;

// 记录接收端收到的总字节数
static void
Measurement(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = sink->GetTotalRxJob(1);  // job1
        
    // 100ms内接收到的字节数
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;

    // 记录总字节数和本100ms内接收的字节数
    SinkBytesStream_job1 << now.GetSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
                            
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

int
main(int argc, char* argv[])
{
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);

    // 在程序开始时打开文件流，使用trunc模式清空文件
    cwndStream_n0_job1.open("atp-result/trace-p2p/n0-job1-cwnd-p2p.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n1_job1.open("atp-result/trace-p2p/n1-job1-cwnd-p2p.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job1.open("atp-result/trace-p2p/n0-job1-sinkBytes-p2p.txt", std::ofstream::out | std::ofstream::trunc);
    

    uint32_t maxBytes = 248;
    Time stopTime = Seconds(1.5);

    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(4);

    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point link required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer d0d2, d1d2, d2d3;
    d0d2 = pointToPoint.Install(n0n2);
    d1d2 = pointToPoint.Install(n1n2);
    d2d3 = pointToPoint.Install(n2n3);


    //
    // Install the internet stack on the nodes
    //
    InternetStackHelper internet;
    internet.Install(nodes);

    //
    // We've got the "hardware" in place.  Now we need to add IP addresses.
    //
    NS_LOG_INFO("Assign IP Addresses.");
    // 使用ipv4Helper作为地址分配器的名字
    Ipv4AddressHelper ipv4Helper;
    ipv4Helper.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4Helper.Assign(d0d2);

    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4Helper.Assign(d1d2);

    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4Helper.Assign(d2d3);

    NS_LOG_INFO("Create Applications.");

    //
    // Create a PacketSinkApplication and install it on node 3
    //

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
    // Create an ATPBulkSendApplication and install it on node 0 and node 1
    //
    Ptr<ATPBulkSendApplication> n0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n1job1App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort = 11;
    Address n0Address(InetSocketAddress(i0i2.GetAddress(0), sendPort));  // 使用端口11
    Address n1Address(InetSocketAddress(i1i2.GetAddress(0), sendPort));  // 使用端口11

    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> n0job1_ATPSocket = DynamicCast<ATPSocket>(n0job1socket);
    Ptr<ATPSocket> n1job1_ATPSocket = DynamicCast<ATPSocket>(n1job1socket);

    n0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n0job1App));
    n0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n0job1App));
    n0job1_ATPSocket->Bind(n0Address);
    n0job1_ATPSocket->Connect(sinkAddress);

    n1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n1job1App));
    n1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n1job1App));
    n1job1_ATPSocket->Bind(n1Address);
    n1job1_ATPSocket->Connect(sinkAddress);

    // 连接拥塞窗口跟踪
    n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job1));
    n1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n1_job1));

    // 配置n0job1App
    n0job1App->Setup(sinkAddress, n0job1_ATPSocket, maxBytes, 1);
    n0job1App->SetEnableATPTag(true);
    n0job1App->SetStartTime(Seconds(1.0));
    n0job1App->SetStopTime(stopTime);
    n0job1App->SetWorkerId(0b00000001);
    nodes.Get(0)->AddApplication(n0job1App);

    // 配置n1job1App
    n1job1App->Setup(sinkAddress, n1job1_ATPSocket, maxBytes, 1);
    n1job1App->SetEnableATPTag(true);
    n1job1App->SetStartTime(Seconds(1.0));
    n1job1App->SetStopTime(stopTime);
    n1job1App->SetWorkerId(0b00000010);
    nodes.Get(1)->AddApplication(n1job1App);

    // 添加地址映射
    sinkATPSocket->AddAddressMapping(1, i0i2.GetAddress(0), sendPort);
    sinkATPSocket->AddAddressMapping(1, i1i2.GetAddress(0), sendPort);

    // 配置静态路由
    // 获取每个节点的Ipv4对象
    Ptr<Ipv4> ipv4_n0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_n1 = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_n2 = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_n3 = nodes.Get(3)->GetObject<Ipv4>();

    // 获取每个节点的静态路由对象
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting_n0 = staticRoutingHelper.GetStaticRouting(ipv4_n0);
    Ptr<Ipv4StaticRouting> staticRouting_n1 = staticRoutingHelper.GetStaticRouting(ipv4_n1);
    Ptr<Ipv4StaticRouting> staticRouting_n2 = staticRoutingHelper.GetStaticRouting(ipv4_n2);
    Ptr<Ipv4StaticRouting> staticRouting_n3 = staticRoutingHelper.GetStaticRouting(ipv4_n3);

    // 配置n0的路由表
    // n0到n3的路由:通过n2转发
    staticRouting_n0->AddHostRouteTo(i2i3.GetAddress(1), i0i2.GetAddress(1), 1);

    // 配置n1的路由表  
    // n1到n3的路由:通过n2转发
    staticRouting_n1->AddHostRouteTo(i2i3.GetAddress(1), i1i2.GetAddress(1), 1);

    // 配置n2的路由表
    // n2到n0的路由
    staticRouting_n2->AddHostRouteTo(i0i2.GetAddress(0), i0i2.GetAddress(0), 1);
    // n2到n1的路由  
    staticRouting_n2->AddHostRouteTo(i1i2.GetAddress(0), i1i2.GetAddress(0), 2);
    // n2到n3的路由
    staticRouting_n2->AddHostRouteTo(i2i3.GetAddress(1), i2i3.GetAddress(1), 3);

    // 配置n3的路由表
    // n3到n0的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(i0i2.GetAddress(0), i2i3.GetAddress(0), 1);
    // n3到n1的路由:通过n2转发
    staticRouting_n3->AddHostRouteTo(i1i2.GetAddress(0), i2i3.GetAddress(0), 1);

    // 开始测量
    Simulator::Schedule(MilliSeconds(100), &Measurement, sinkApp);
    
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
    SinkBytesStream_job1.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
