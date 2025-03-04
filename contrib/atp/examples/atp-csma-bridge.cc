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
//#include "ns3/packet-sink.h"
#include "ns3/atp-module.h" // atp module
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/atp-csma-helper.h"
#include "ns3/atp-csma-channel.h"
#include "ns3/atp-csma-net-device.h"

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

    LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    LogComponentEnable("ATPBridgeNetDevice", LOG_LEVEL_ALL);
    LogComponentEnable("ATPTxBuffer", LOG_LEVEL_ALL);
    LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("ATPPacketSink", LOG_LEVEL_ALL);

    bool tracing = false;
    uint64_t maxBytes = 248;

    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(3);

    NodeContainer csmaSwitch;
    csmaSwitch.Create(1);

    NS_LOG_INFO("Build Topology");
    ATPCsmaHelper csma;
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
    // 创建PacketSink应用及其socket
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(Ipv4Address("10.1.1.3"), port));
    
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);

    // 使用ATPPacketSink而不是PacketSink
    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();
    sinkApp->SetSocket(sinkATPSocket);
    sinkApp->SetAddressPort(sinkAddress, port);
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(Seconds(20.0));

    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    nodes.Get(2)->AddApplication(sinkApp);

    // 为每个发送应用创建socket并设置跟踪
    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    
    Ptr<ATPSocket> n0job1_ATPSocket = DynamicCast<ATPSocket>(n0job1socket);
    Ptr<ATPSocket> n1job1_ATPSocket = DynamicCast<ATPSocket>(n1job1socket);

    // 创建并配置应用
    Ptr<ATPBulkSendApplication> n0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n1job1App = CreateObject<ATPBulkSendApplication>();

    // 配置n0job1App
    n0job1App->Setup(sinkAddress, n0job1_ATPSocket, maxBytes, 1);
    n0job1App->SetEnableATPTag(true);
    n0job1App->SetStartTime(Seconds(1.0));
    n0job1App->SetStopTime(Seconds(10.0));

    n0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n0job1App),
                                    MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n0job1App));
    n0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n0job1App));
    n0job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(0)->AddApplication(n0job1App);

    // 配置n1job1App
    n1job1App->Setup(sinkAddress, n1job1_ATPSocket, maxBytes, 1);
    n1job1App->SetEnableATPTag(true);
    n1job1App->SetStartTime(Seconds(1.0));
    n1job1App->SetStopTime(Seconds(10.0));

    n1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n1job1App),
                                    MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n1job1App));
    n1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n1job1App));
    n1job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(1)->AddApplication(n1job1App);

    // 连接拥塞窗口跟踪
    n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job1));
    n1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n1_job1));

    // 设置地址映射关系以支持聚合包的多ACK
    NS_LOG_INFO("Setting up address mapping for job ID 1");
    Ipv4Address n0Addr("10.1.1.1");
    Ipv4Address n1Addr("10.1.1.2");
    
    // 为节点0添加节点1的地址映射(job ID 1)
    n0job1_ATPSocket->AddAddressMapping(1, n0Addr, 9);
    n0job1_ATPSocket->AddAddressMapping(1, n1Addr, 9);  // 端口9是应用层端口
    
    // 为节点1添加节点0的地址映射(job ID 1)
    n1job1_ATPSocket->AddAddressMapping(1, n0Addr, 9);
    n1job1_ATPSocket->AddAddressMapping(1, n1Addr, 9);
    NS_LOG_INFO("Address mapping completed");

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

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
