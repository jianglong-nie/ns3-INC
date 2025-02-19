/*
 * Copyright (c) 2008 INRIA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "atp-csma-helper.h"

#include "ns3/abort.h"
#include "ns3/config.h"
#include "ns3/atp-csma-channel.h"
#include "ns3/atp-csma-net-device.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-factory.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/trace-helper.h"

#include <string>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPCsmaHelper");

ATPCsmaHelper::ATPCsmaHelper()
{
    m_queueFactory.SetTypeId("ns3::DropTailQueue<Packet>");
    m_deviceFactory.SetTypeId("ns3::ATPCsmaNetDevice");
    m_channelFactory.SetTypeId("ns3::ATPCsmaChannel");
    m_enableFlowControl = true;
}

void
ATPCsmaHelper::SetDeviceAttribute(std::string n1, const AttributeValue& v1)
{
    m_deviceFactory.Set(n1, v1);
}

void
ATPCsmaHelper::SetChannelAttribute(std::string n1, const AttributeValue& v1)
{
    m_channelFactory.Set(n1, v1);
}

void
ATPCsmaHelper::DisableFlowControl()
{
    m_enableFlowControl = false;
}

void
ATPCsmaHelper::EnablePcapInternal(std::string prefix,
                               Ptr<NetDevice> nd,
                               bool promiscuous,
                               bool explicitFilename)
{
    //
    // All of the Pcap enable functions vector through here including the ones
    // that are wandering through all of devices on perhaps all of the nodes in
    // the system.  We can only deal with devices of type ATPCsmaNetDevice.
    //
    Ptr<ATPCsmaNetDevice> device = nd->GetObject<ATPCsmaNetDevice>();
    if (!device)
    {
        NS_LOG_INFO("ATPCsmaHelper::EnablePcapInternal(): Device "
                    << device << " not of type ns3::ATPCsmaNetDevice");
        return;
    }

    PcapHelper pcapHelper;

    std::string filename;
    if (explicitFilename)
    {
        filename = prefix;
    }
    else
    {
        filename = pcapHelper.GetFilenameFromDevice(prefix, device);
    }

    Ptr<PcapFileWrapper> file =
        pcapHelper.CreateFile(filename, std::ios::out, PcapHelper::DLT_EN10MB);
    if (promiscuous)
    {
        pcapHelper.HookDefaultSink<ATPCsmaNetDevice>(device, "PromiscSniffer", file);
    }
    else
    {
        pcapHelper.HookDefaultSink<ATPCsmaNetDevice>(device, "Sniffer", file);
    }
}

void
ATPCsmaHelper::EnableAsciiInternal(Ptr<OutputStreamWrapper> stream,
                                std::string prefix,
                                Ptr<NetDevice> nd,
                                bool explicitFilename)
{
    //
    // All of the ascii enable functions vector through here including the ones
    // that are wandering through all of devices on perhaps all of the nodes in
    // the system.  We can only deal with devices of type ATPCsmaNetDevice.
    //
    Ptr<ATPCsmaNetDevice> device = nd->GetObject<ATPCsmaNetDevice>();
    if (!device)
    {
        NS_LOG_INFO("ATPCsmaHelper::EnableAsciiInternal(): Device "
                    << device << " not of type ns3::ATPCsmaNetDevice");
        return;
    }

    //
    // Our default trace sinks are going to use packet printing, so we have to
    // make sure that is turned on.
    //
    Packet::EnablePrinting();

    //
    // If we are not provided an OutputStreamWrapper, we are expected to create
    // one using the usual trace filename conventions and do a Hook*WithoutContext
    // since there will be one file per context and therefore the context would
    // be redundant.
    //
    if (!stream)
    {
        //
        // Set up an output stream object to deal with private ofstream copy
        // constructor and lifetime issues.  Let the helper decide the actual
        // name of the file given the prefix.
        //
        AsciiTraceHelper asciiTraceHelper;

        std::string filename;
        if (explicitFilename)
        {
            filename = prefix;
        }
        else
        {
            filename = asciiTraceHelper.GetFilenameFromDevice(prefix, device);
        }

        Ptr<OutputStreamWrapper> theStream = asciiTraceHelper.CreateFileStream(filename);

        //
        // The MacRx trace source provides our "r" event.
        //
        asciiTraceHelper.HookDefaultReceiveSinkWithoutContext<ATPCsmaNetDevice>(device,
                                                                             "MacRx",
                                                                             theStream);

        //
        // The "+", '-', and 'd' events are driven by trace sources actually in the
        // transmit queue.
        //
        Ptr<Queue<Packet>> queue = device->GetQueue();
        asciiTraceHelper.HookDefaultEnqueueSinkWithoutContext<Queue<Packet>>(queue,
                                                                             "Enqueue",
                                                                             theStream);
        asciiTraceHelper.HookDefaultDropSinkWithoutContext<Queue<Packet>>(queue, "Drop", theStream);
        asciiTraceHelper.HookDefaultDequeueSinkWithoutContext<Queue<Packet>>(queue,
                                                                             "Dequeue",
                                                                             theStream);

        return;
    }

    //
    // If we are provided an OutputStreamWrapper, we are expected to use it, and
    // to providd a context.  We are free to come up with our own context if we
    // want, and use the AsciiTraceHelper Hook*WithContext functions, but for
    // compatibility and simplicity, we just use Config::Connect and let it deal
    // with the context.
    //
    // Note that we are going to use the default trace sinks provided by the
    // ascii trace helper.  There is actually no AsciiTraceHelper in sight here,
    // but the default trace sinks are actually publicly available static
    // functions that are always there waiting for just such a case.
    //
    uint32_t nodeid = nd->GetNode()->GetId();
    uint32_t deviceid = nd->GetIfIndex();
    std::ostringstream oss;

    oss << "/NodeList/" << nd->GetNode()->GetId() << "/DeviceList/" << deviceid
        << "/$ns3::ATPCsmaNetDevice/MacRx";
    Config::Connect(oss.str(),
                    MakeBoundCallback(&AsciiTraceHelper::DefaultReceiveSinkWithContext, stream));

    oss.str("");
    oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid
        << "/$ns3::ATPCsmaNetDevice/TxQueue/Enqueue";
    Config::Connect(oss.str(),
                    MakeBoundCallback(&AsciiTraceHelper::DefaultEnqueueSinkWithContext, stream));

    oss.str("");
    oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid
        << "/$ns3::ATPCsmaNetDevice/TxQueue/Dequeue";
    Config::Connect(oss.str(),
                    MakeBoundCallback(&AsciiTraceHelper::DefaultDequeueSinkWithContext, stream));

    oss.str("");
    oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid
        << "/$ns3::ATPCsmaNetDevice/TxQueue/Drop";
    Config::Connect(oss.str(),
                    MakeBoundCallback(&AsciiTraceHelper::DefaultDropSinkWithContext, stream));
}

NetDeviceContainer
ATPCsmaHelper::Install(Ptr<Node> node) const
{
    Ptr<ATPCsmaChannel> channel = m_channelFactory.Create()->GetObject<ATPCsmaChannel>();
    return Install(node, channel);
}

NetDeviceContainer
ATPCsmaHelper::Install(std::string nodeName) const
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    return Install(node);
}

NetDeviceContainer
ATPCsmaHelper::Install(Ptr<Node> node, Ptr<ATPCsmaChannel> channel) const
{
    return NetDeviceContainer(InstallPriv(node, channel));
}

NetDeviceContainer
ATPCsmaHelper::Install(Ptr<Node> node, std::string channelName) const
{
    Ptr<ATPCsmaChannel> channel = Names::Find<ATPCsmaChannel>(channelName);
    return NetDeviceContainer(InstallPriv(node, channel));
}

NetDeviceContainer
ATPCsmaHelper::Install(std::string nodeName, Ptr<ATPCsmaChannel> channel) const
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    return NetDeviceContainer(InstallPriv(node, channel));
}

NetDeviceContainer
ATPCsmaHelper::Install(std::string nodeName, std::string channelName) const
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    Ptr<ATPCsmaChannel> channel = Names::Find<ATPCsmaChannel>(channelName);
    return NetDeviceContainer(InstallPriv(node, channel));
}

NetDeviceContainer
ATPCsmaHelper::Install(const NodeContainer& c) const
{
    Ptr<ATPCsmaChannel> channel = m_channelFactory.Create()->GetObject<ATPCsmaChannel>();

    return Install(c, channel);
}

NetDeviceContainer
ATPCsmaHelper::Install(const NodeContainer& c, Ptr<ATPCsmaChannel> channel) const
{
    NetDeviceContainer devs;

    for (auto i = c.Begin(); i != c.End(); i++)
    {
        devs.Add(InstallPriv(*i, channel));
    }

    return devs;
}

NetDeviceContainer
ATPCsmaHelper::Install(const NodeContainer& c, std::string channelName) const
{
    Ptr<ATPCsmaChannel> channel = Names::Find<ATPCsmaChannel>(channelName);
    return Install(c, channel);
}

int64_t
ATPCsmaHelper::AssignStreams(NetDeviceContainer c, int64_t stream)
{
    int64_t currentStream = stream;
    Ptr<NetDevice> netDevice;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        netDevice = (*i);
        Ptr<ATPCsmaNetDevice> csma = DynamicCast<ATPCsmaNetDevice>(netDevice);
        if (csma)
        {
            currentStream += csma->AssignStreams(currentStream);
        }
    }
    return (currentStream - stream);
}

Ptr<NetDevice>
ATPCsmaHelper::InstallPriv(Ptr<Node> node, Ptr<ATPCsmaChannel> channel) const
{
    Ptr<ATPCsmaNetDevice> device = m_deviceFactory.Create<ATPCsmaNetDevice>();
    device->SetAddress(Mac48Address::Allocate());
    node->AddDevice(device);
    Ptr<Queue<Packet>> queue = m_queueFactory.Create<Queue<Packet>>();
    device->SetQueue(queue);
    device->Attach(channel);
    if (m_enableFlowControl)
    {
        // Aggregate a NetDeviceQueueInterface object
        Ptr<NetDeviceQueueInterface> ndqi = CreateObject<NetDeviceQueueInterface>();
        ndqi->GetTxQueue(0)->ConnectQueueTraces(queue);
        device->AggregateObject(ndqi);
    }
    return device;
}

} // namespace ns3
