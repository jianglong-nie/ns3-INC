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
    : m_faninDegree0(0),
      m_faninDegree1(0),
      m_bitmap0(0),
      m_bitmap1(0),
      m_jobId(0),
      m_seqNum(0),
      m_ecn(0),
      m_isAck(0),
      m_overflow(0),
      m_resend(0),
      m_collision(0),
      m_edgeSwitchIdentifier(0),
      m_size(0),
      m_sourcePort(0xfffd),
      m_destinationPort(0xfffd)
{
    NS_LOG_FUNCTION(this);
}

void
ATPTag::SetFaninDegree0(uint8_t faninDegree0)
{
    NS_LOG_FUNCTION(this << faninDegree0);
    m_faninDegree0 = faninDegree0;
}

uint8_t
ATPTag::GetFaninDegree0() const
{
    NS_LOG_FUNCTION(this);
    return m_faninDegree0;
}

void
ATPTag::SetFaninDegree1(uint8_t faninDegree1)
{
    NS_LOG_FUNCTION(this << faninDegree1);
    m_faninDegree1 = faninDegree1;
}

uint8_t
ATPTag::GetFaninDegree1() const
{
    NS_LOG_FUNCTION(this);
    return m_faninDegree1;
}

void
ATPTag::SetBitMap0(uint32_t bitmap0)
{
    NS_LOG_FUNCTION(this << bitmap0);
    m_bitmap0 = bitmap0;
}

uint32_t
ATPTag::GetBitMap0() const
{
    NS_LOG_FUNCTION(this);
    return m_bitmap0;
}

void
ATPTag::SetBitMap1(uint32_t bitmap1)
{
    NS_LOG_FUNCTION(this << bitmap1);
    m_bitmap1 = bitmap1;
}

uint32_t
ATPTag::GetBitMap1() const
{
    NS_LOG_FUNCTION(this);
    return m_bitmap1;
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
ATPTag::SetSeqNum(uint32_t seqNum)
{
    NS_LOG_FUNCTION(this << seqNum);
    m_seqNum = seqNum;
}

uint32_t
ATPTag::GetSeqNum() const
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
    return  1 + 1 + 4 + 4 + 1 + 4 + 1 + 1 + 1 + 1 + 1 + 1 + 2 + 2 + 2;
    // faninDegree0 + faninDegree1 + bitmap0 + bitmap1 + jobId + seqNum 
    // + ecn + isAck + overflow + resend + collision + edgeSwitchIdentifier + size + sourcePort + destinationPort
}

void
ATPTag::Serialize(TagBuffer buf) const
{
    NS_LOG_FUNCTION(this << &buf);
    buf.WriteU8(m_faninDegree0);
    buf.WriteU8(m_faninDegree1);
    buf.WriteU32(m_bitmap0);
    buf.WriteU32(m_bitmap1);
    buf.WriteU8(m_jobId);
    buf.WriteU32(m_seqNum);
    buf.WriteU8(m_ecn);
    buf.WriteU8(m_isAck);
    buf.WriteU8(m_overflow);
    buf.WriteU8(m_resend);
    buf.WriteU8(m_collision);
    buf.WriteU8(m_edgeSwitchIdentifier);
    buf.WriteU16(m_size);
    buf.WriteU16(m_sourcePort);
    buf.WriteU16(m_destinationPort);
}

void
ATPTag::Deserialize(TagBuffer buf)
{
    NS_LOG_FUNCTION(this << &buf);
    m_faninDegree0 = buf.ReadU8();
    m_faninDegree1 = buf.ReadU8();
    m_bitmap0 = buf.ReadU32();
    m_bitmap1 = buf.ReadU32();
    m_jobId = buf.ReadU8();
    m_seqNum = buf.ReadU32();
    m_ecn = buf.ReadU8();
    m_isAck = buf.ReadU8();
    m_overflow = buf.ReadU8();
    m_resend = buf.ReadU8();
    m_collision = buf.ReadU8();
    m_edgeSwitchIdentifier = buf.ReadU8();
    m_size = buf.ReadU16();
    m_sourcePort = buf.ReadU16();
    m_destinationPort = buf.ReadU16();
}

void ATPTag::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << " faninDegree0=" << static_cast<int>(m_faninDegree0)
       << " faninDegree1=" << static_cast<int>(m_faninDegree1)
       << " bitmap0=" << m_bitmap0
       << " bitmap1=" << m_bitmap1
       << " seqNum=" << static_cast<int>(m_seqNum)
       << " jobId=" << static_cast<int>(m_jobId)
       << " ecn=" << static_cast<int>(m_ecn)
       << " isAck=" << static_cast<int>(m_isAck)
       << " overflow=" << static_cast<int>(m_overflow)
       << " resend=" << static_cast<int>(m_resend)
       << " collision =" << static_cast<int>(m_collision)
       << " edgeSwitchIdentifier" << static_cast<int>(m_edgeSwitchIdentifier)
       << " size=" << m_size
       << " sourcePort=" << m_sourcePort
       << " destinationPort=" << m_destinationPort
       << ")";
}

void
ATPTag::CopyFrom(const ATPTag& other)
{
    m_faninDegree0 = other.m_faninDegree0;
    m_faninDegree1 = other.m_faninDegree1;
    m_bitmap0 = other.m_bitmap0;
    m_bitmap1 = other.m_bitmap1;
    m_jobId = other.m_jobId;
    m_seqNum = other.m_seqNum;
    m_ecn = other.m_ecn;
    m_isAck = other.m_isAck;
    m_overflow = other.m_overflow;
    m_resend = other.m_resend;
    m_collision = other.m_collision;
    m_edgeSwitchIdentifier = other.m_edgeSwitchIdentifier;
    m_size = other.m_size;
    m_sourcePort = other.m_sourcePort;
    m_destinationPort = other.m_destinationPort;
}

} // namespace ns3
