/*
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#include "atp-tag.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPTag");

NS_OBJECT_ENSURE_REGISTERED(ATPTag);

TypeId
ATPTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ATPTag")
                           .SetParent<Tag>()
                           .SetGroupName("Internet")
                           .AddConstructor<ATPTag>();
    return tid;
}

TypeId
ATPTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

ATPTag::ATPTag()
    : m_atpPacketType(ATPTag::UNKNOWN),
      m_jobId(0),
      m_seqNum(0),
      m_ackNum(0),
      m_ecn(0),
      m_size(0),
      m_sourcePort(0xfffd),
      m_destinationPort(0xfffd)
{
    NS_LOG_FUNCTION(this);
}

void
ATPTag::SetPacketType(uint8_t type)
{
    NS_LOG_FUNCTION(this << static_cast<int>(type));
    m_atpPacketType = type;
}

uint8_t
ATPTag::GetPacketType() const
{
    NS_LOG_FUNCTION(this);
    return m_atpPacketType;
}

void
ATPTag::SetJobId(uint8_t jobId)
{
    NS_LOG_FUNCTION(this << jobId);
    m_jobId = jobId;
}

uint8_t
ATPTag::GetJobId() const
{
    NS_LOG_FUNCTION(this);
    return m_jobId;
}

void
ATPTag::SetSeqNumber(uint8_t seqNum)
{
    NS_LOG_FUNCTION(this << seqNum);
    m_seqNum = seqNum;
}

void
ATPTag::SetAckNumber(uint8_t ackNum)
{
    NS_LOG_FUNCTION(this << ackNum);
    m_ackNum = ackNum;
}

uint8_t
ATPTag::GetAckNumber() const
{
    NS_LOG_FUNCTION(this);
    return m_ackNum;
}

uint8_t
ATPTag::GetSeqNumber() const
{
    NS_LOG_FUNCTION(this);
    return m_seqNum;
}

void
ATPTag::SetSize(uint16_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_size = size;
}

uint16_t
ATPTag::GetSize() const
{
    NS_LOG_FUNCTION(this);
    return m_size;
}

void
ATPTag::SetEcn(uint8_t ecn)
{
    NS_LOG_FUNCTION(this << ecn);
    m_ecn = ecn;
}

uint8_t
ATPTag::GetEcn() const
{
    NS_LOG_FUNCTION(this);
    return m_ecn;
}

void
ATPTag::SetSourcePort(uint16_t sourcePort)
{
    NS_LOG_FUNCTION(this << sourcePort);
    m_sourcePort = sourcePort;
}

uint16_t
ATPTag::GetSourcePort() const
{
    NS_LOG_FUNCTION(this);
    return m_sourcePort;
}

void
ATPTag::SetDestinationPort(uint16_t destinationPort)
{
    NS_LOG_FUNCTION(this << destinationPort);
    m_destinationPort = destinationPort;
}

uint16_t
ATPTag::GetDestinationPort() const
{
    NS_LOG_FUNCTION(this);
    return m_destinationPort;
}

uint32_t
ATPTag::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return  1 + 1 + 1 + 1 + 1 + 2 + 2 + 2;
    // packetType + jobId + seqNum + ackNum + ecn + size + sourcePort + destinationPort
}

void
ATPTag::Serialize(TagBuffer buf) const
{
    NS_LOG_FUNCTION(this << &buf);
    buf.WriteU8(m_atpPacketType);
    buf.WriteU8(m_jobId);
    buf.WriteU8(m_seqNum);
    buf.WriteU8(m_ackNum);
    buf.WriteU8(m_ecn);
    buf.WriteU16(m_size);
    buf.WriteU16(m_sourcePort);
    buf.WriteU16(m_destinationPort);
}

void
ATPTag::Deserialize(TagBuffer buf)
{
    NS_LOG_FUNCTION(this << &buf);
    m_atpPacketType = buf.ReadU8();
    m_jobId = buf.ReadU8();
    m_seqNum = buf.ReadU8();
    m_ackNum = buf.ReadU8();
    m_ecn = buf.ReadU8();
    m_size = buf.ReadU16();
    m_sourcePort = buf.ReadU16();
    m_destinationPort = buf.ReadU16();
}

void ATPTag::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << " packetType=" << static_cast<int>(m_atpPacketType)
       << " (seqNum=" << static_cast<int>(m_seqNum)
       << " jobId=" << static_cast<int>(m_jobId)
       << " ackNum=" << static_cast<int>(m_ackNum)
       << " ecn=" << static_cast<int>(m_ecn)
       << " size=" << m_size
       << " sourcePort=" << m_sourcePort
       << " destinationPort=" << m_destinationPort
       << ")";
}

} // namespace ns3
