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
    // Create an ATPBulkSendApplication and install it on node 0 and node 1
    //
    uint16_t port = 9; // well-known echo port number
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), port));

    ATPBulkSendHelper source0("ns3::TcpSocketFactory", sinkAddress);
    ATPBulkSendHelper source1("ns3::TcpSocketFactory", sinkAddress);
    // Set the amount of data to send in bytes.  Zero is unlimited.
    source0.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    source1.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    //source.SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
    ApplicationContainer sourceApps0 = source0.Install(nodes.Get(0));
    ApplicationContainer sourceApps1 = source1.Install(nodes.Get(1));
    sourceApps0.Start(Seconds(0.0));
    sourceApps0.Stop(Seconds(10.0));
    sourceApps1.Start(Seconds(0.0));
    sourceApps1.Stop(Seconds(10.0));

    //
    // Create a PacketSinkApplication and install it on node 3
    //
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    //
    // Set up tracing if enabled
    //
    if (tracing)
    {
        AsciiTraceHelper ascii;
        pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-bulk-send.tr"));
        pointToPoint.EnablePcapAll("tcp-bulk-send", false);
    }

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << std::endl;

    return 0;
}
