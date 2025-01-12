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

    void SetJobId(uint8_t jobId);
    uint8_t GetJobId() const;

    void SetSeqNumber(uint8_t seqNum);
    uint8_t GetSeqNumber() const;

    void SetSize(uint16_t size);
    uint16_t GetSize() const;

    // Inherited from Tag
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer buf) const override;
    void Deserialize(TagBuffer buf) override;
    void Print(std::ostream& os) const override;

  private:
    uint8_t m_jobId{0};    //!< Job ID
    uint8_t m_seqNum{0};   //!< Sequence number
    uint16_t m_size{0};    //!< Size of data to send each time
};

} // namespace ns3

#endif /* ATP_TAG_H */
