#include "atp-aggregator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("Aggregator");

Aggregator::Aggregator()
    : m_packet(nullptr)
{
    NS_LOG_FUNCTION(this);
}

Aggregator::~Aggregator()
{
    NS_LOG_FUNCTION(this);
    
    // 如果m_packet不为空，释放它
    m_packet = nullptr;
}

bool
Aggregator::ProcessPacket(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // 获取ATPTag
    ATPTag atpTag;
    packet->PeekPacketTag(atpTag);
    packet->RemovePacketTag(atpTag);

    if (m_packet == nullptr) {
        m_packet = packet->Copy();
    }

    uint8_t temp_faninDegree = 0;
    uint32_t temp_bitmap = 0;
    uint8_t temp_edgeSwitchIdentifier = 0;
    if (atpTag.m_edgeSwitchIdentifier == 0) {
        temp_bitmap = atpTag.m_bitmap0;
        temp_faninDegree = atpTag.m_faninDegree0;
        temp_edgeSwitchIdentifier = atpTag.m_edgeSwitchIdentifier;
        atpTag.m_edgeSwitchIdentifier += 1;
    }
    else {
        temp_bitmap = atpTag.m_bitmap1;
        temp_faninDegree = atpTag.m_faninDegree1;
        temp_edgeSwitchIdentifier = atpTag.m_edgeSwitchIdentifier;
        atpTag.m_edgeSwitchIdentifier = 0;
    }

    // 判断该包是否被聚合过了, temp_bitmap的某个bit为1,判断m_bitmap某个位置bit是否为1
    if ((temp_bitmap & m_bitmap) == temp_bitmap){
        m_ecn = atpTag.m_ecn;
        return false;
    }
    else {
        m_bitmap = m_bitmap | temp_bitmap;
        m_count++;
        if (m_count == temp_faninDegree) {
            if (temp_edgeSwitchIdentifier == 0) {
                atpTag.m_bitmap0 = m_bitmap;
            }
            else if (temp_edgeSwitchIdentifier == 1) {
                atpTag.m_bitmap1 = m_bitmap;
            }
            else {
                NS_LOG_ERROR("Edge switch identifier is invalid!");
            }
            m_packet = packet->Copy();
            m_packet->AddPacketTag(atpTag);
            return true;
        }
        else {
            m_ecn = atpTag.m_ecn;
            return false;
        }
    }
}

Ptr<Packet>
Aggregator::GetResultPacket() const
{
    NS_LOG_FUNCTION(this);
    return m_packet;
}

void
Aggregator::Reset()
{
    NS_LOG_FUNCTION(this);
    m_bitmap = 0;
    m_count = 0;
    m_jobId = 0;
    m_seqNum = 0;
    m_packet = nullptr;
}

} // namespace ns3