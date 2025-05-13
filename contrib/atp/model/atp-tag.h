/*
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef ATP_TAG_H
#define ATP_TAG_H

#include "ns3/tag.h"

namespace ns3
{

/**
 * \brief ATP tag to carry sequence number, job ID and size
 *
 * This tag contains sequence number, job ID and size information for local 
 * packet processing only and is not transmitted over the network.
 */
class ATPTag : public Tag
{
  public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    ATPTag();

    void SetFaninDegree(uint8_t faninDegree);
    uint8_t GetFaninDegree() const;

    void SetWorkerId(uint8_t workerId);
    uint8_t GetWorkerId() const;

    // 定义数据包类型
    // 使用具有描述性名称的常量，但底层仍然是 uint8_t
    static constexpr uint8_t UNKNOWN = 0;
    static constexpr uint8_t DATA = 1;
    static constexpr uint8_t ACK = 2;
    static constexpr uint8_t AGG = 3;

    void SetPacketType(uint8_t type);
    uint8_t GetPacketType() const;

    void SetJobId(uint8_t jobId);
    uint8_t GetJobId() const;

    void SetSeqNumber(uint32_t seqNum);
    uint32_t GetSeqNumber() const;

    void SetAckNumber(uint32_t ackNum);
    uint32_t GetAckNumber() const;

    void SetSize(uint16_t size);
    uint16_t GetSize() const;

    void SetEcn(uint8_t ecn);
    uint8_t GetEcn() const;

    void SetSourcePort(uint16_t sourcePort);
    uint16_t GetSourcePort() const;

    void SetDestinationPort(uint16_t destinationPort);
    uint16_t GetDestinationPort() const;

    // Inherited from Tag
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer buf) const override;
    void Deserialize(TagBuffer buf) override;
    void Print(std::ostream& os) const override;

    // 添加友元运算符
    friend std::ostream& operator<<(std::ostream& os, const ATPTag& tag)
    {
        tag.Print(os);
        return os;
    }

    // 添加复制函数
    void CopyFrom(const ATPTag& other);

  private:
    uint8_t m_faninDegree{0};
    uint8_t m_workerId{0};
    uint8_t m_atpPacketType{0};
    uint8_t m_jobId{0};    //!< Job ID
    uint32_t m_seqNum{0};   //!< Sequence number
    uint32_t m_ackNum{0};   //!< Ack number
    uint8_t m_ecn{0};      //!< ECN
    uint16_t m_size{0};    //!< Size of data to send each time
    uint16_t m_sourcePort{0xfffd};      //!< Source port
    uint16_t m_destinationPort{0xfffd}; //!< Destination port
};

} // namespace ns3

#endif /* ATP_TAG_H */
