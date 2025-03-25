/*
 * Copyright 2007 University of Washington
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */
#include "atp-packet-sink.h"

#include "ns3/address-utils.h"
#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/ipv6-packet-info-tag.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-socket.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPPacketSink");

NS_OBJECT_ENSURE_REGISTERED(ATPPacketSink);

TypeId
ATPPacketSink::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ATPPacketSink")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<ATPPacketSink>()
            .AddAttribute("Local",
                          "The Address on which to Bind the rx socket.",
                          AddressValue(),
                          MakeAddressAccessor(&ATPPacketSink::m_local),
                          MakeAddressChecker())
            .AddAttribute("Protocol",
                          "The type id of the protocol to use for the rx socket.",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&ATPPacketSink::m_tid),
                          MakeTypeIdChecker())
            .AddAttribute("EnableSeqTsSizeHeader",
                          "Enable optional header tracing of SeqTsSizeHeader",
                          BooleanValue(false),
                          MakeBooleanAccessor(&ATPPacketSink::m_enableSeqTsSizeHeader),
                          MakeBooleanChecker())
            .AddTraceSource("Rx",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&ATPPacketSink::m_rxTrace),
                            "ns3::Packet::AddressTracedCallback")
            .AddTraceSource("RxWithAddresses",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&ATPPacketSink::m_rxTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("RxWithSeqTsSize",
                            "A packet with SeqTsSize header has been received",
                            MakeTraceSourceAccessor(&ATPPacketSink::m_rxTraceWithSeqTsSize),
                            "ns3::ATPPacketSink::SeqTsSizeCallback");
    return tid;
}

ATPPacketSink::ATPPacketSink()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;
    m_totalRx = 0;
}

ATPPacketSink::~ATPPacketSink()
{
    NS_LOG_FUNCTION(this);
}

void
ATPPacketSink::SetSocket(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    m_socket = socket;
}

void
ATPPacketSink::SetAddressPort(Address address, uint16_t port)
{
    NS_LOG_FUNCTION(this << address << port);
    m_local = address;
    m_localPort = port;
}

uint64_t
ATPPacketSink::GetTotalRx() const
{
    NS_LOG_FUNCTION(this);
    return m_totalRx;
}

uint64_t
ATPPacketSink::GetTotalRxJob(uint8_t jobId) const
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(jobId));
    auto it = m_jobRx.find(jobId);
    if (it != m_jobRx.end()) {
        return it->second;
    }
    return 0;
}

Ptr<Socket>
ATPPacketSink::GetListeningSocket() const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

std::list<Ptr<Socket>>
ATPPacketSink::GetAcceptedSockets() const
{
    NS_LOG_FUNCTION(this);
    return m_socketList;
}

void
ATPPacketSink::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;
    m_socketList.clear();

    // chain up
    Application::DoDispose();
}

// Application Methods
void
ATPPacketSink::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);
    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        NS_ABORT_MSG_IF(m_local.IsInvalid(), "'Local' attribute not properly set");
        if (m_socket->Bind(m_local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Listen();
        //m_socket->ShutdownSend();
        if (addressUtils::IsMulticast(m_local))
        {
            Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket>(m_socket);
            if (udpSocket)
            {
                // equivalent to setsockopt (MCAST_JOIN_GROUP)
                udpSocket->MulticastJoinGroup(0, m_local);
            }
            else
            {
                NS_FATAL_ERROR("Error: joining multicast on a non-UDP socket");
            }
        }
    }

    if (InetSocketAddress::IsMatchingType(m_local))
    {
        m_localPort = InetSocketAddress::ConvertFrom(m_local).GetPort();
    }
    else if (Inet6SocketAddress::IsMatchingType(m_local))
    {
        m_localPort = Inet6SocketAddress::ConvertFrom(m_local).GetPort();
    }
    else
    {
        m_localPort = 0;
    }
    m_socket->SetRecvCallback(MakeCallback(&ATPPacketSink::HandleRead, this));
    m_socket->SetRecvPktInfo(true);
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                MakeCallback(&ATPPacketSink::HandleAccept, this));
    m_socket->SetCloseCallbacks(MakeCallback(&ATPPacketSink::HandlePeerClose, this),
                                MakeCallback(&ATPPacketSink::HandlePeerError, this));
}

void
ATPPacketSink::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);
    while (!m_socketList.empty()) // these are accepted sockets, close them
    {
        Ptr<Socket> acceptedSocket = m_socketList.front();
        m_socketList.pop_front();
        acceptedSocket->Close();
    }
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void
ATPPacketSink::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
        if (packet->GetSize() == 0)
        { // EOF
            break;
        }

        // Get ATP tag to identify job
        ATPTag atpTag;
        bool found = packet->PeekPacketTag(atpTag);
        
        // Update total bytes received
        m_totalRx += packet->GetSize();
        
        // Update job-specific bytes received
        if (found) {
            uint8_t jobId = atpTag.GetJobId();
            m_jobRx[jobId] += packet->GetSize();
            NS_LOG_INFO("Job " << static_cast<uint32_t>(jobId) 
                       << " received " << packet->GetSize() 
                       << " bytes, total: " << m_jobRx[jobId]);
        }

        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " packet sink received "
                                   << packet->GetSize() << " bytes from "
                                   << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(from).GetPort() << " total Rx "
                                   << m_totalRx << " bytes");
        }
        else if (Inet6SocketAddress::IsMatchingType(from))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " packet sink received "
                                   << packet->GetSize() << " bytes from "
                                   << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " port "
                                   << Inet6SocketAddress::ConvertFrom(from).GetPort()
                                   << " total Rx " << m_totalRx << " bytes");
        }

        if (!m_rxTrace.IsEmpty() || !m_rxTraceWithAddresses.IsEmpty() ||
            (!m_rxTraceWithSeqTsSize.IsEmpty() && m_enableSeqTsSizeHeader))
        {
            Ipv4PacketInfoTag interfaceInfo;
            Ipv6PacketInfoTag interface6Info;
            if (packet->RemovePacketTag(interfaceInfo))
            {
                localAddress = InetSocketAddress(interfaceInfo.GetAddress(), m_localPort);
            }
            else if (packet->RemovePacketTag(interface6Info))
            {
                localAddress = Inet6SocketAddress(interface6Info.GetAddress(), m_localPort);
            }
            else
            {
                socket->GetSockName(localAddress);
            }
            m_rxTrace(packet, from);
            m_rxTraceWithAddresses(packet, from, localAddress);

            if (!m_rxTraceWithSeqTsSize.IsEmpty() && m_enableSeqTsSizeHeader)
            {
                PacketReceived(packet, from, localAddress);
            }
        }
    }
}

void
ATPPacketSink::PacketReceived(const Ptr<Packet>& p, const Address& from, const Address& localAddress)
{
    SeqTsSizeHeader header;
    Ptr<Packet> buffer;

    auto itBuffer = m_buffer.find(from);
    if (itBuffer == m_buffer.end())
    {
        itBuffer = m_buffer.insert(std::make_pair(from, Create<Packet>(0))).first;
    }

    buffer = itBuffer->second;
    buffer->AddAtEnd(p);
    buffer->PeekHeader(header);

    NS_ABORT_IF(header.GetSize() == 0);

    while (buffer->GetSize() >= header.GetSize())
    {
        NS_LOG_DEBUG("Removing packet of size " << header.GetSize() << " from buffer of size "
                                                << buffer->GetSize());
        Ptr<Packet> complete = buffer->CreateFragment(0, static_cast<uint32_t>(header.GetSize()));
        buffer->RemoveAtStart(static_cast<uint32_t>(header.GetSize()));

        complete->RemoveHeader(header);

        m_rxTraceWithSeqTsSize(complete, from, localAddress, header);

        if (buffer->GetSize() > header.GetSerializedSize())
        {
            buffer->PeekHeader(header);
        }
        else
        {
            break;
        }
    }
}

void
ATPPacketSink::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
ATPPacketSink::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
ATPPacketSink::HandleAccept(Ptr<Socket> s, const Address& from)
{
    NS_LOG_FUNCTION(this << s << from);
    s->SetRecvCallback(MakeCallback(&ATPPacketSink::HandleRead, this));
    m_socketList.push_back(s);
}

} // Namespace ns3
