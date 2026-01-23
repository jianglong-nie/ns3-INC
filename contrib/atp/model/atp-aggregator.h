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

    uint8_t m_ecn = 0;
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
    
    /*
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
    */
    
    // 支持layerId的重载版本，使用boost::hash_combine风格
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        std::size_t seed = 0;
        boost::hash_combine(seed, jobId);
        boost::hash_combine(seed, seqNum);
        return seed % MAX_AGGREGATORS;
    }
    /*
    static std::size_t HashToIndex(uint8_t jobId, uint32_t seqNum, uint32_t MAX_AGGREGATORS) {
        // 使用大质数来获得更好的分布
        std::size_t hash = (jobId * 2654435761UL) ^ (seqNum * 2246822519UL);
        return hash % MAX_AGGREGATORS;
    }
    
    
    // 支持layerId的重载版本，使用boost::hash_combine风格
    static std::size_t HashToIndexLayer(uint8_t jobId, uint32_t seqNum, uint8_t layerId, uint32_t MAX_AGGREGATORS) {
        std::size_t seed = 0;
        boost::hash_combine(seed, jobId);
        boost::hash_combine(seed, seqNum);
        boost::hash_combine(seed, layerId);
        return seed % MAX_AGGREGATORS;
    }
    */

    // 不依赖boost::hash_combine的分层哈希版本，确保不同layerId在哈希上有区别
    static std::size_t HashToIndexLayer(uint8_t jobId, uint32_t seqNum, uint8_t layerId, uint32_t MAX_AGGREGATORS) {
        // 使用不同大质数混合各个域，确保分层扰动且分布较均匀
        std::size_t hash =
            (static_cast<std::size_t>(jobId) * 2654435761UL)
            ^ (static_cast<std::size_t>(seqNum) * 2246822519UL)
            ^ (static_cast<std::size_t>(layerId) * 3266489917UL);
        return hash % MAX_AGGREGATORS;
    }
        
};

} // namespace ns3

#endif /* ATP_AGGREGATOR_H */