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
    : m_jobId(0),
      m_seqNum(0),
      m_size(0)
{
    NS_LOG_FUNCTION(this);
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

uint32_t
ATPTag::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 4; // seqNum(1) + jobId(1) + size(2)
}

void
ATPTag::Serialize(TagBuffer buf) const
{
    NS_LOG_FUNCTION(this << &buf);
    buf.WriteU8(m_seqNum);
    buf.WriteU8(m_jobId);
    buf.WriteU16(m_size);
}

void
ATPTag::Deserialize(TagBuffer buf)
{
    NS_LOG_FUNCTION(this << &buf);
    m_seqNum = buf.ReadU8();
    m_jobId = buf.ReadU8();
    m_size = buf.ReadU16();
}

void
ATPTag::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "(seqNum=" << m_seqNum << " jobId=" << m_jobId << " size=" << m_size << ")";
}

} // namespace ns3
