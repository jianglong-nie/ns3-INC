/*
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "atp-header.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPHeader");

NS_OBJECT_ENSURE_REGISTERED(ATPHeader);

TypeId
ATPHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ATPHeader")
                           .SetParent<Header>()
                           .SetGroupName("Internet")
                           .AddConstructor<ATPHeader>();
    return tid;
}

TypeId
ATPHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

ATPHeader::ATPHeader()
    : m_packetType(UNKNOWN),
      m_jobId(0),
      m_seqNum(0),
      m_ackNum(0),
      m_size(0),
      m_windowSize(0xffff),
      m_sourcePort(0xfffd),
      m_destinationPort(0xfffd)
{
    NS_LOG_FUNCTION(this);
}

void
ATPHeader::SetPacketType(PacketType type)
{
    NS_LOG_FUNCTION(this << static_cast<int>(type));
    m_packetType |= static_cast<uint8_t>(type);
}

void
ATPHeader::ClearPacketType(PacketType type)
{
    NS_LOG_FUNCTION(this << static_cast<int>(type));
    m_packetType &= ~static_cast<uint8_t>(type);
}

uint8_t
ATPHeader::GetPacketType() const
{
    NS_LOG_FUNCTION(this);
    return m_packetType;
}

void
ATPHeader::SetJobId(uint8_t jobId)
{
    NS_LOG_FUNCTION(this << jobId);
    m_jobId = jobId;
}

uint8_t
ATPHeader::GetJobId() const
{
    NS_LOG_FUNCTION(this);
    return m_jobId;
}

void
ATPHeader::SetSeqNumber(uint32_t seqNum)
{
    NS_LOG_FUNCTION(this << seqNum);
    m_seqNum = seqNum;
}

uint8_t
ATPHeader::GetSeqNumber() const
{
    NS_LOG_FUNCTION(this);
    return m_seqNum; 
}

void
ATPHeader::SetAckNumber(uint8_t ackNumber)
{
    NS_LOG_FUNCTION(this << ackNumber);
    m_ackNum = ackNumber;
}

uint8_t
ATPHeader::GetAckNumber() const
{
    NS_LOG_FUNCTION(this);
    return m_ackNum;
}

void
ATPHeader::SetWindowSize(uint16_t windowSize)
{
    NS_LOG_FUNCTION(this << windowSize);
    m_windowSize = windowSize;
}

uint16_t
ATPHeader::GetWindowSize() const
{
    NS_LOG_FUNCTION(this);
    return m_windowSize;
}

void
ATPHeader::SetSize(uint16_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_size = size;
}

uint16_t
ATPHeader::GetSize() const
{
    NS_LOG_FUNCTION(this);
    return m_size;
}

void
ATPHeader::SetSourcePort(uint16_t port)
{
    NS_LOG_FUNCTION(this << port);
    m_sourcePort = port;
}

uint16_t
ATPHeader::GetSourcePort() const
{
    NS_LOG_FUNCTION(this);
    return m_sourcePort;
}

void
ATPHeader::SetDestinationPort(uint16_t port)
{
    NS_LOG_FUNCTION(this << port);
    m_destinationPort = port;
}

uint16_t
ATPHeader::GetDestinationPort() const
{
    NS_LOG_FUNCTION(this);
    return m_destinationPort;
}

void
ATPHeader::SetSourceAddress(const Address& source)
{
    NS_LOG_FUNCTION(this << source);
    m_source = source;
}

Address
ATPHeader::GetSourceAddress() const
{
    NS_LOG_FUNCTION(this);
    return m_source;
}

void
ATPHeader::SetDestinationAddress(const Address& destination)
{
    NS_LOG_FUNCTION(this << destination);
    m_destination = destination;
}

Address
ATPHeader::GetDestinationAddress() const
{
    NS_LOG_FUNCTION(this);
    return m_destination;
}

uint32_t
ATPHeader::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 1 + // packet type
           1 + // seq
           1 + // ack number
           1 + // job id
           2 + // window size
           2 + // source port
           2 + // destination port
           4;  // size
}

void
ATPHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    i.WriteU8(m_packetType);
    i.WriteU8(m_seqNum);
    i.WriteU8(m_ackNum);
    i.WriteU8(m_jobId);
    i.WriteHtonU16(m_windowSize);
    i.WriteHtonU32(m_size);
    i.WriteHtonU16(m_sourcePort);
    i.WriteHtonU16(m_destinationPort);
}

uint32_t
ATPHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    m_packetType = i.ReadU8();
    m_seqNum = i.ReadU8();
    m_ackNum = i.ReadU8();
    m_jobId = i.ReadU8();
    m_windowSize = i.ReadNtohU16();
    m_size = i.ReadNtohU32();
    m_sourcePort = i.ReadNtohU16();
    m_destinationPort = i.ReadNtohU16();
    return GetSerializedSize();
}

void
ATPHeader::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    std::string flags;
    if (m_packetType & DATA) flags += "DATA ";
    if (m_packetType & ACK) flags += "ACK ";
    if (m_packetType & AGG) flags += "AGG ";
    if (m_packetType & ECN) flags += "ECN ";
    if (m_packetType == UNKNOWN) flags = "UNKNOWN";
    
    os << "(type=" << flags
       << " seq=" << static_cast<uint32_t>(m_seqNum)
       << " ack=" << static_cast<uint32_t>(m_ackNum)
       << " jobId=" << static_cast<uint32_t>(m_jobId)
       << " window=" << m_windowSize
       << " size=" << m_size << ")";
}

} // namespace ns3
