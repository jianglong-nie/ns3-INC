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

    void SetFaninDegree0(uint8_t faninDegree0);
    uint8_t GetFaninDegree0() const;

    void SetFaninDegree1(uint8_t faninDegree1);
    uint8_t GetFaninDegree1() const;

    void SetBitMap0(uint32_t bitmap0);
    uint32_t GetBitMap0() const;

    void SetBitMap1(uint32_t bitmap1);
    uint32_t GetBitMap1() const;

    void SetJobId(uint8_t jobId);
    uint8_t GetJobId() const;

    void SetSeqNum(uint32_t seqNum);
    uint32_t GetSeqNum() const;

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

    uint8_t m_faninDegree0{0};
    uint8_t m_faninDegree1{0};
    uint32_t m_bitmap0{0};
    uint32_t m_bitmap1{0};
    uint8_t m_jobId{0};    //!< Job ID
    uint32_t m_seqNum{0};   //!< Sequence number
    uint8_t m_ecn{0};      //!< ECN
    uint8_t m_isAck{0};
    uint8_t m_overflow{0};
    uint8_t m_resend{0};
    uint8_t m_collision{0};
    uint8_t m_edgeSwitchIdentifier{0};
    uint16_t m_size{0};    //!< Size of data to send each time
    uint16_t m_sourcePort{0xfffd};      //!< Source port
    uint16_t m_destinationPort{0xfffd}; //!< Destination port
};

} // namespace ns3

#endif /* ATP_TAG_H */
