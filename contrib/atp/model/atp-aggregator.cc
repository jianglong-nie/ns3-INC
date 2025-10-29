#include "atp-aggregator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("Aggregator");

Aggregator::Aggregator()
    : m_jobId(0),
      m_seqNum(0),
      m_faninDegree(0b00000000),
      m_bitmap(0b00000000),
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

    if (m_packet == nullptr)
    {
        m_packet = packet->Copy();
    }

    if (atpTag.m_edgeSwitchIdentifier == 0) {
        m_bitmap = m_bitmap | atpTag.m_bitmap0;
        if (m_bitmap == m_faninDegree) {
            m_packet->RemovePacketTag(atpTag);
            atpTag.SetBitMap0(m_bitmap);
            atpTag.m_edgeSwitchIdentifier += 1;
            m_packet->AddPacketTag(atpTag);
            return true;  // 聚合完成，可以取出数据包了
        }
        return false;
    }
    else {
        m_bitmap = m_bitmap | atpTag.m_bitmap1;
        if (m_bitmap == m_faninDegree) {
            m_packet->RemovePacketTag(atpTag);
            atpTag.SetBitMap1(m_bitmap);
            m_packet->AddPacketTag(atpTag);
            return true;  // 聚合完成，可以取出数据包了
        }
        return false;
    }
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
    m_bitmap = 0b00000000;
    m_faninDegree = 0b00000000;
    m_jobId = 0;
    m_seqNum = 0;
    m_packet = nullptr;
}

} // namespace ns3