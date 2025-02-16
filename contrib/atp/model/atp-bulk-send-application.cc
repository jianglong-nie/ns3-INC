/*
 * Copyright (c) 2010 Georgia Institute of Technology
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

#include "atp-bulk-send-application.h"

#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/atp-socket.h"
#include "ns3/atp-socket-factory.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPBulkSendApplication");

NS_OBJECT_ENSURE_REGISTERED(ATPBulkSendApplication);

TypeId
ATPBulkSendApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ATPBulkSendApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<ATPBulkSendApplication>()
            .AddAttribute("SendSize",
                          "The amount of data to send each time.",
                          UintegerValue(248), // 每次发送的数据量默认值最大为248字节，正如atp论文中所写到的
                          MakeUintegerAccessor(&ATPBulkSendApplication::m_sendSize),
                          MakeUintegerChecker<uint32_t>(1)) // 最小值为1
            .AddAttribute("Remote",
                          "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&ATPBulkSendApplication::m_peer),
                          MakeAddressChecker())
            .AddAttribute("Local",
                          "The Address on which to bind the socket. If not set, it is generated "
                          "automatically.",
                          AddressValue(),
                          MakeAddressAccessor(&ATPBulkSendApplication::m_local),
                          MakeAddressChecker())
            .AddAttribute("MaxBytes",
                          "The total number of bytes to send. "
                          "Once these bytes are sent, "
                          "no data  is sent again. The value zero means "
                          "that there is no limit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&ATPBulkSendApplication::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Protocol",
                          "The type of protocol to use.",
                          TypeIdValue(ATPSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&ATPBulkSendApplication::m_tid),
                          MakeTypeIdChecker())
            .AddAttribute("EnableATPTag",
                          "Add ATPTag to each packet",
                          BooleanValue(false),
                          MakeBooleanAccessor(&ATPBulkSendApplication::m_enableATPTag),
                          MakeBooleanChecker())
            .AddAttribute("JobId",
                          "The job ID",
                          UintegerValue(0),
                          MakeUintegerAccessor(&ATPBulkSendApplication::m_jobId),
                          MakeUintegerChecker<uint32_t>())
            .AddTraceSource("Tx",
                            "A new packet is sent",
                            MakeTraceSourceAccessor(&ATPBulkSendApplication::m_txTrace),
                            "ns3::Packet::TracedCallback");
            /*.AddTraceSource("ATPRetransmission",
                            "The ATP socket retransmitted a packet",
                            MakeTraceSourceAccessor(&ATPBulkSendApplication::m_retransmissionTrace),
                            "ns3::ATPSocket::RetransmissionCallback");*/

    return tid;
}

ATPBulkSendApplication::ATPBulkSendApplication()
    : m_socket(nullptr),
      m_connected(false),
      m_totBytes(0),
      m_unsentPacket(nullptr)
{
    NS_LOG_FUNCTION(this);
}

ATPBulkSendApplication::~ATPBulkSendApplication()
{
    NS_LOG_FUNCTION(this);
}

void
ATPBulkSendApplication::SetMaxBytes(uint64_t maxBytes)
{
    NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
ATPBulkSendApplication::GetSocket() const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

void
ATPBulkSendApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_socket = nullptr;
    m_unsentPacket = nullptr;
    // chain up
    Application::DoDispose();
}

// Application Methods
void
ATPBulkSendApplication::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);
    Address from;

    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        int ret = -1;

        // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
        if (m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM &&
            m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET)
        {
            NS_FATAL_ERROR("Using BulkSend with an incompatible socket type. "
                           "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                           "In other words, use TCP instead of UDP.");
        }

        NS_ABORT_MSG_IF(m_peer.IsInvalid(), "'Remote' attribute not properly set");

        if (!m_local.IsInvalid())
        {
            NS_ABORT_MSG_IF((Inet6SocketAddress::IsMatchingType(m_peer) &&
                             InetSocketAddress::IsMatchingType(m_local)) ||
                                (InetSocketAddress::IsMatchingType(m_peer) &&
                                 Inet6SocketAddress::IsMatchingType(m_local)),
                            "Incompatible peer and local address IP version");
            ret = m_socket->Bind(m_local);
        }
        else
        {
            if (Inet6SocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind6();
            }
            else if (InetSocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind();
            }
        }

        if (ret == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, this),
                                     MakeCallback(&ATPBulkSendApplication::ConnectionFailed, this));
        m_socket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, this));
        m_socket->Connect(m_peer);
        //m_socket->ShutdownRecv(); 
        /*Ptr<ATPSocket> atpSocket = DynamicCast<ATPSocket>(m_socket);
        if (atpSocket)
        {
            atpSocket->TraceConnectWithoutContext(
                "Retransmission",
                MakeCallback(&ATPBulkSendApplication::PacketRetransmitted, this));
        }*/
    }
    if (m_connected)
    {
        //m_socket->GetSockName(from);
        SendData(from, m_peer);
    }
}

void
ATPBulkSendApplication::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
        m_connected = false;
    }
    else
    {
        NS_LOG_WARN("BulkSendApplication found null socket to close in StopApplication");
    }
}

// Private helpers

void
ATPBulkSendApplication::SendData(const Address& from, const Address& to)
{
    NS_LOG_FUNCTION(this);

    // 如果m_maxBytes为0，则一直发送数据
    // 如果m_maxBytes不为0，累积m_totBytes，直到m_totBytes达到m_maxBytes，则停止发送
    while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more

        // uint64_t to allow the comparison later.
        // the result is in a uint32_t range anyway, because
        // m_sendSize is uint32_t.
        uint64_t toSend = m_sendSize;
        // Make sure we don't send too many
        if (m_maxBytes > 0)
        {
            toSend = std::min(toSend, m_maxBytes - m_totBytes);
        }

        // 如果m_unsentPacket不为空，则表示上次发送的数据未成功，需要重新发送
        // 如果m_unsentPacket为空，则表示上次发送的数据成功，需要创建新的数据包

        NS_LOG_LOGIC("sending packet at " << Simulator::Now());

        Ptr<Packet> packet;
        if (m_unsentPacket)
        {
            packet = m_unsentPacket;
            toSend = packet->GetSize();
        }
        else if (m_enableATPTag)
        {
            ATPTag tag;
            tag.SetJobId(m_jobId);
            m_seq++; // 序列号自增，初值0，加加后从1开始计数
            tag.SetSeqNumber(m_seq);
            tag.SetSize(toSend);
            packet = Create<Packet>(toSend);
            packet->AddPacketTag(tag);
             // Trace before adding tag, for consistency with PacketSink
            m_txTrace(packet);
        }
        else
        {
            packet = Create<Packet>(toSend);
        }

        int actual = m_socket->Send(packet);
        if ((unsigned)actual == toSend)
        {
            m_totBytes += actual;
            m_txTrace(packet);
            m_unsentPacket = nullptr;
        }
        else if (actual == -1)
        {
            // We exit this loop when actual < toSend as the send side
            // buffer is full. The "DataSent" callback will pop when
            // some buffer space has freed up.
            NS_LOG_DEBUG("Unable to send packet; caching for later attempt");
            m_unsentPacket = packet;
            break;
        }
        else if (actual > 0 && (unsigned)actual < toSend)
        {
            // A Linux socket (non-blocking, such as in DCE) may return
            // a quantity less than the packet size.  Split the packet
            // into two, trace the sent packet, save the unsent packet
            NS_LOG_DEBUG("Packet size: " << packet->GetSize() << "; sent: " << actual
                                         << "; fragment saved: " << toSend - (unsigned)actual);
            Ptr<Packet> sent = packet->CreateFragment(0, actual);
            Ptr<Packet> unsent = packet->CreateFragment(actual, (toSend - (unsigned)actual));
            m_totBytes += actual;
            m_txTrace(sent);
            m_unsentPacket = unsent;
            break;
        }
        else
        {
            NS_FATAL_ERROR("Unexpected return value from m_socket->Send ()");
        }
    }
    // Check if time to close (all sent)
    
    /*if (m_totBytes == m_maxBytes && m_connected)
    {
        m_socket->Close();
        m_connected = false;
    }*/
}

void
ATPBulkSendApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_LOGIC("ATPBulkSendApplication Connection succeeded");
    m_connected = true;
    Address from;
    Address to;
    socket->GetSockName(from);
    socket->GetPeerName(to);
    //SendData(from, to);
}

void
ATPBulkSendApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_LOG_LOGIC("BulkSendApplication, Connection Failed");
}

void
ATPBulkSendApplication::DataSend(Ptr<Socket> socket, uint32_t)
{
    NS_LOG_FUNCTION(this);

    if (m_connected)
    { // Only send new data if the connection has completed
        Address from;
        Address to;
        socket->GetSockName(from);
        socket->GetPeerName(to);
        SendData(from, to);
    }
}

/*void
ATPBulkSendApplication::PacketRetransmitted(Ptr<const Packet> p,
                                         const ATPHeader& header,
                                         const Address& localAddr,
                                         const Address& peerAddr,
                                         Ptr<const ATPSocket> socket)
{
    NS_LOG_FUNCTION(this << p << header << localAddr << peerAddr << socket);
    m_retransmissionTrace(p, header, localAddr, peerAddr, socket);
}*/


void
ATPBulkSendApplication::SetJobId(uint32_t jobId)
{
    m_jobId = jobId;
}

uint32_t
ATPBulkSendApplication::GetJobId() const
{
    return m_jobId;
}

void
ATPBulkSendApplication::Setup(Address sinkAddress, Ptr<Socket> socket, uint64_t maxBytes, uint32_t jobId)
{   
    m_peer = sinkAddress;
    m_socket = socket;
    m_maxBytes = maxBytes;
    m_jobId = jobId;
}

void
ATPBulkSendApplication::SetEnableATPTag(bool enableATPTag)
{
    m_enableATPTag = enableATPTag;
}

bool
ATPBulkSendApplication::GetEnableATPTag() const
{
    return m_enableATPTag;
}

void
ATPBulkSendApplication::SetSocket(Ptr<Socket> socket)
{
    m_socket = socket;
}

} // Namespace ns3
