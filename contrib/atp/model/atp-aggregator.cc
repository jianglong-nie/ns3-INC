#include "atp-aggregator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("Aggregator");

Aggregator::Aggregator()
    : m_jobId(0),
      m_seqNum(0),
      m_count(0),
      m_faninDegree(0b00000000),
      m_workerIdAgg(0b00000000),
      m_packet(nullptr)
{
    NS_LOG_FUNCTION(this);
}

Aggregator::~Aggregator()
{
    NS_LOG_FUNCTION(this);
    
    // 如果m_packet不为空，释放它
    m_packet = nullptr;
}

void
Aggregator::SetFaninDegree(uint8_t faninDegree)
{
    NS_LOG_FUNCTION(this << faninDegree);
    m_faninDegree = faninDegree;
}

bool
Aggregator::AddPacket(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // 获取ATPTag
    ATPTag atpTag;
    packet->PeekPacketTag(atpTag);
    uint8_t workerId = atpTag.GetWorkerId();

    if (m_packet == nullptr)
    {
        m_packet = packet->Copy();
    }

    m_count++;
    m_workerIdAgg = m_workerIdAgg | workerId;

    // 检查是否完成聚合
    if (m_workerIdAgg == m_faninDegree)
    {
        m_packet->RemovePacketTag(atpTag);
        atpTag.SetWorkerId(m_workerIdAgg);
        m_packet->AddPacketTag(atpTag);
        return true;  // 聚合完成，可以取出数据包了
    }
    
    return false;  // 聚合未完成，继续收集数据包
}

Ptr<Packet>
Aggregator::GetAggregatedPacket() const
{
    NS_LOG_FUNCTION(this);
    return m_packet;
}

void
Aggregator::Reset()
{
    NS_LOG_FUNCTION(this);
    m_workerIdAgg = 0b00000000;
    //m_faninDegree = 0b00000000;
    m_jobId = 0;
    m_seqNum = 0;
    m_count = 0;
    m_packet = nullptr;
}

} // namespace ns3