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

#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATPBulkSendExample");

int
main(int argc, char* argv[])
{
    LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);

    bool tracing = false;
    uint32_t maxBytes = 2000;
    Time stopTime = Seconds(20.0);

    //
    // Allow the user to override any of the defaults at
    // run-time, via command-line arguments
    //
    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Flag to enable/disable tracing", tracing);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.Parse(argc, argv);

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
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

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
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

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
    // n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
    //      MakeCallback(&CwndChange_n0_job1));
    // n1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
    //      MakeCallback(&CwndChange_n1_job1));

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
