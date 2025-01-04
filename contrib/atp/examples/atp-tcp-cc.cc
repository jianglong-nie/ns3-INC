/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
//
//        n0     n1
//        |      |
//       ----------
//       | Switch |
//       ----------
//        |      
//        n2                    
//
//
// - Flow from n0 to n1 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "tcp-bulk-send.tr"
//   and pcap tracing available when tracing is turned on.

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/atp-module.h" // atp module
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/tcp-congestion-ops.h"

#include <fstream>
#include <string>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATPBulkCsmaExample");

std::ofstream cwndStream_n0_job1;
std::ofstream cwndStream_n1_job1;

static void
CwndChange_n0_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n0_job1 << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n1_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n1_job1 << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

int
main(int argc, char* argv[])
{
    // 在程序开始时打开文件流，使用trunc模式清空文件
    cwndStream_n0_job1.open("n0-job1-cwnd.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n1_job1.open("n1-job1-cwnd.txt", std::ofstream::out | std::ofstream::trunc);

    //LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    //LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    //LogComponentEnable("ATPSocketBase", LOG_LEVEL_INFO);
    //LogComponentEnable("ATPBridgeNetDevice", LOG_LEVEL_ALL);
    LogComponentEnable("ATPCC", LOG_LEVEL_ALL);
    bool tracing = false;
    uint64_t maxBytes = 100000000;

    // 直接设置拥塞控制算法
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(ATPCC::GetTypeId()));
    // Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    // Config::SetDefault("ns3::TcpBbr::HighGain", DoubleValue(2.89));
    // Config::SetDefault("ns3::TcpBbr::BwWindowLength", UintegerValue(10));

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
    nodes.Create(3);

    NodeContainer csmaSwitch;
    csmaSwitch.Create(1);

    NS_LOG_INFO("Build Topology");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer nodesDevices;
    NetDeviceContainer switchDevices;

    for (int i = 0; i < 3; i++)
    {
        NetDeviceContainer link = csma.Install(NodeContainer(nodes.Get(i), csmaSwitch));
        nodesDevices.Add(link.Get(0));
        switchDevices.Add(link.Get(1));
    }

    // Create the bridge netdevice, which will do the packet switching
    Ptr<Node> switchNode = csmaSwitch.Get(0);
    ATPBridgeHelper bridge;
    bridge.Install(switchNode, switchDevices);

    // Install the internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // We've got the "hardware" in place.  Now we need to add IP addresses.
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(nodesDevices);

    NS_LOG_INFO("Create Applications.");
    // 创建sink应用
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(Ipv4Address("10.1.1.3"), port));
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    // 为每个发送应用创建socket并设置跟踪
    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<Socket> n1job1socket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());

    n0job1socket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange_n0_job1));
    n1job1socket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange_n1_job1));

    // 创建并配置应用
    Ptr<ATPBulkSendApplication> n0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n1job1App = CreateObject<ATPBulkSendApplication>();

    // 配置n0job1App
    n0job1App->Setup(sinkAddress, n0job1socket, maxBytes, 1); // maxBytes和jobId=1
    n0job1App->SetEnableATPTag(true);
    n0job1App->SetStartTime(Seconds(0.0));
    n0job1App->SetStopTime(Seconds(10.0));
    nodes.Get(0)->AddApplication(n0job1App);

    // 配置n1job1App
    n1job1App->Setup(sinkAddress, n1job1socket, maxBytes, 1);
    n1job1App->SetEnableATPTag(true);
    n1job1App->SetStartTime(Seconds(0.0));
    n1job1App->SetStopTime(Seconds(10.0));
    nodes.Get(1)->AddApplication(n1job1App);

    // Set up tracing if enabled
    if (tracing)
    {
        AsciiTraceHelper ascii;
        csma.EnableAsciiAll(ascii.CreateFileStream("atp-bulk-csma.tr"));
        csma.EnablePcapAll("atp-bulk-csma", false);
    }

    // Now, do the actual simulation.
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    cwndStream_n0_job1.close();
    cwndStream_n1_job1.close();

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << std::endl;

    return 0;
}
