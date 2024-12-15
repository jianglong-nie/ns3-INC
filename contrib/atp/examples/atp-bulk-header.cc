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

#include <fstream>
#include <string>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATPBulkCsmaExample");

int
main(int argc, char* argv[])
{
    LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    //LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    bool tracing = false;
    uint32_t maxBytes = 2000;

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
    BridgeHelper bridge;
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
    // Create an ATPBulkSendApplication and install it on node 0 and node 1
    // send data to node 2
    // Set the amount of data to send in bytes.  Zero is unlimited.
    // source.SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
    uint16_t port = 9; // well-known echo port number
    Address sinkAddress(InetSocketAddress(Ipv4Address("10.1.1.3"), port));
    uint32_t jobId1 = 1;
    uint32_t jobId2 = 2;
    ATPBulkSendHelper job1("ns3::TcpSocketFactory", sinkAddress);
    ATPBulkSendHelper job2("ns3::TcpSocketFactory", sinkAddress);
    job1.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    job1.SetAttribute("EnableATPHeader", BooleanValue(true));
    job1.SetAttribute("JobId", UintegerValue(jobId1));

    job2.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    job2.SetAttribute("EnableATPHeader", BooleanValue(true));
    job2.SetAttribute("JobId", UintegerValue(jobId2));

    ApplicationContainer n0job1 = job1.Install(nodes.Get(0));
    ApplicationContainer n0job2 = job2.Install(nodes.Get(0));
    ApplicationContainer n1job1 = job1.Install(nodes.Get(1));
    ApplicationContainer n1job2 = job2.Install(nodes.Get(1));

    n0job1.Start(Seconds(0.0));
    n0job1.Stop(Seconds(10.0));
    n1job1.Start(Seconds(0.0));
    n1job1.Stop(Seconds(10.0));

    n0job2.Start(Seconds(0.0));
    n0job2.Stop(Seconds(10.0));
    n1job2.Start(Seconds(0.0));
    n1job2.Stop(Seconds(10.0));
    

    // Create a PacketSinkApplication and install it on node 3
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

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

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << std::endl;

    return 0;
}
