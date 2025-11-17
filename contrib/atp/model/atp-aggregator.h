#ifndef ATP_AGGREGATOR_H
#define ATP_AGGREGATOR_H

#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "atp-tag.h"
#include <boost/functional/hash.hpp>
#include <map>

#include <cstdint>

namespace ns3 {

class Aggregator
{
public:
    Aggregator();
    ~Aggregator();

    void SetFaninDegree(uint8_t jobId, uint8_t faninDegree);

    bool AddPacket(Ptr<const Packet> packet);
    Ptr<Packet> GetAggregatedPacket() const;

    uint8_t m_jobId = 0;
    uint32_t m_seqNum = 0;
    uint32_t m_count = 0;
    // uint8_t m_faninDegree = 0b00000000;
    // 使用map来存储jobId和faninDegree
    std::map<uint8_t, uint8_t> m_jobFaninDegree; // jobId: faninDegree
    uint8_t m_workerIdAgg = 0b00000000;
    Ptr<Packet> m_packet;   //!< 存储的数据包

    bool IsEmpty() const {return m_count == 0;};
    void Reset();

    // 添加静态哈希函数
    
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        std::size_t h1 = std::hash<uint8_t>()(jobId);
        std::size_t h2 = std::hash<uint32_t>()(seqNum);
        return ((h1 << 1) ^ h2) % MAX_AGGREGATORS;
    }
    
    // 分层哈希函数：不同层使用不同的哈希计算，避免连环冲突
    static std::size_t HashToIndexLayer(uint8_t jobId, uint32_t seqNum, uint8_t layerId, uint32_t MAX_AGGREGATORS) {
        std::size_t h1 = std::hash<uint8_t>()(jobId);
        std::size_t h2 = std::hash<uint32_t>()(seqNum);
        std::size_t h3 = std::hash<uint8_t>()(layerId);
        
        // 使用layerId改变哈希计算方式，不同层产生不同的映射
        return ((h1 << layerId) ^ h2 ^ (h3 << 2)) % MAX_AGGREGATORS;
    }
    
    /*
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        std::size_t seed = 0;
        boost::hash_combine(seed, jobId);
        boost::hash_combine(seed, seqNum);
        return seed % MAX_AGGREGATORS;
    }
    */
        
};

} // namespace ns3

#endif /* ATP_AGGREGATOR_H */