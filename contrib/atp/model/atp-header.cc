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

ATPHeader::ATPHeader()
    : m_seq(0),
      m_jobId(0),
      m_ts(Simulator::Now().GetTimeStep()),
      m_size(0)
{
    NS_LOG_FUNCTION(this);
}

void
ATPHeader::SetSeq(uint32_t seq)
{
    NS_LOG_FUNCTION(this << seq);
    m_seq = seq;
}

uint32_t
ATPHeader::GetSeq() const
{
    NS_LOG_FUNCTION(this);
    return m_seq;
}

void
ATPHeader::SetJobId(uint32_t jobId)
{
    NS_LOG_FUNCTION(this << jobId);
    m_jobId = jobId;
}

uint32_t
ATPHeader::GetJobId() const
{
    NS_LOG_FUNCTION(this);
    return m_jobId;
}

Time
ATPHeader::GetTs() const
{
    NS_LOG_FUNCTION(this);
    return TimeStep(m_ts);
}

void
ATPHeader::SetSize(uint64_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_size = size;
}

uint64_t
ATPHeader::GetSize() const
{
    NS_LOG_FUNCTION(this);
    return m_size;
}

TypeId
ATPHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ATPHeader")
                           .SetParent<Header>()
                           .SetGroupName("ATP")
                           .AddConstructor<ATPHeader>();
    return tid;
}

TypeId
ATPHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
ATPHeader::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "(seq=" << m_seq << " jobId=" << m_jobId << " time=" << TimeStep(m_ts).As(Time::S)
       << " size=" << m_size << ")";
}

uint32_t
ATPHeader::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 4 + 4 + 8 + 8; // seq(4) + jobId(4) + timestamp(8) + size(8)
}

void
ATPHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    i.WriteHtonU32(m_seq);
    i.WriteHtonU32(m_jobId);
    i.WriteHtonU64(m_ts);
    i.WriteHtonU64(m_size);
}

uint32_t
ATPHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);
    Buffer::Iterator i = start;
    m_seq = i.ReadNtohU32();
    m_jobId = i.ReadNtohU32();
    m_ts = i.ReadNtohU64();
    m_size = i.ReadNtohU64();
    return GetSerializedSize();
}

} // namespace ns3
