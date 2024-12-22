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
                           .SetGroupName("ATP")
                           .AddConstructor<ATPTag>();
    return tid;
}

TypeId
ATPTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

ATPTag::ATPTag()
    : m_seq(0),
      m_jobId(0),
      m_size(0)
{
    NS_LOG_FUNCTION(this);
}

void
ATPTag::SetSeq(uint32_t seq)
{
    NS_LOG_FUNCTION(this << seq);
    m_seq = seq;
}

uint32_t
ATPTag::GetSeq() const
{
    NS_LOG_FUNCTION(this);
    return m_seq;
}

void
ATPTag::SetJobId(uint32_t jobId)
{
    NS_LOG_FUNCTION(this << jobId);
    m_jobId = jobId;
}

uint32_t
ATPTag::GetJobId() const
{
    NS_LOG_FUNCTION(this);
    return m_jobId;
}

void
ATPTag::SetSize(uint64_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_size = size;
}

uint64_t
ATPTag::GetSize() const
{
    NS_LOG_FUNCTION(this);
    return m_size;
}

uint32_t
ATPTag::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 4 + 4 + 8; // seq(4) + jobId(4) + size(8)
}

void
ATPTag::Serialize(TagBuffer buf) const
{
    NS_LOG_FUNCTION(this << &buf);
    buf.WriteU32(m_seq);
    buf.WriteU32(m_jobId);
    buf.WriteU64(m_size);
}

void
ATPTag::Deserialize(TagBuffer buf)
{
    NS_LOG_FUNCTION(this << &buf);
    m_seq = buf.ReadU32();
    m_jobId = buf.ReadU32();
    m_size = buf.ReadU64();
}

void
ATPTag::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "(seq=" << m_seq << " jobId=" << m_jobId << " size=" << m_size << ")";
}

} // namespace ns3
