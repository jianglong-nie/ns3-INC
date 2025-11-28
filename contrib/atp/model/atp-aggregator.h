#ifndef ATP_AGGREGATOR_H
#define ATP_AGGREGATOR_H

#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "atp-tag.h"
#include <boost/functional/hash.hpp>

#include <cstdint>

namespace ns3 {

class Aggregator
{
public:
    Aggregator();
    ~Aggregator();

    bool ProcessPacket(Ptr<Packet> packet);
    Ptr<Packet> GetResultPacket() const;

    uint8_t m_ecn = 0;
    uint32_t m_bitmap = 0;
    uint32_t m_count = 0;
    uint8_t m_jobId = 0;
    uint32_t m_seqNum = 0;
    Ptr<Packet> m_packet = nullptr;   //!< 存储的数据包

    bool IsEmpty() const {return m_count == 0;};
    void Reset();

    // 添加静态哈希函数
    /*
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        std::size_t h1 = std::hash<uint8_t>()(jobId);
        std::size_t h2 = std::hash<uint32_t>()(seqNum);
        return ((h1 << 1) ^ h2) % MAX_AGGREGATORS;
    }
    */
   /*
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        std::size_t seed = 0;
        boost::hash_combine(seed, jobId);
        boost::hash_combine(seed, seqNum);
        return seed % MAX_AGGREGATORS;
    }
    */
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        // 使用大质数来获得更好的分布
        std::size_t hash = (jobId * 2654435761UL) ^ (seqNum * 2246822519UL);
        return hash % MAX_AGGREGATORS;
    }
        
};

class JobAggregator
{
public:
    JobAggregator();
    ~JobAggregator();
    
    // 处理收到的包，更新 flat_bitmap
    bool ProcessPacket(Ptr<Packet> packet, uint32_t incoming_flat_bitmap);
    Ptr<Packet> GetResultPacket() const;

    uint32_t m_flat_bitmap = 0;       //!< 扁平化的 bitmap，跟踪哪些 worker 已到达
    uint8_t m_jobId = 0;
    uint32_t m_seqNum = 0;
    Ptr<Packet> m_packet = nullptr;   //!< 存储的数据包

    bool IsEmpty() const {return m_flat_bitmap == 0;};
    
    // 检查是否完成聚合
    bool IsComplete(uint32_t expected_flat_bitmap) const {
        return m_flat_bitmap == expected_flat_bitmap;
    };
    
    void Reset();

    /*
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        std::size_t h1 = std::hash<uint8_t>()(jobId);
        std::size_t h2 = std::hash<uint32_t>()(seqNum);
        return ((h1 << 1) ^ h2) % MAX_AGGREGATORS;
    }
    */

    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        std::size_t seed = 0;
        boost::hash_combine(seed, jobId);
        boost::hash_combine(seed, seqNum);
        return seed % MAX_AGGREGATORS;
    }
};

} // namespace ns3

#endif /* ATP_AGGREGATOR_H */