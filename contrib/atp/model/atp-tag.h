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

    /**
     * Create an empty ATP tag
     */
    ATPTag();

    // Inherited from Tag
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer buf) const override;
    void Deserialize(TagBuffer buf) override;
    void Print(std::ostream& os) const override;

    /**
     * \param seq the sequence number
     */
    void SetSeq(uint32_t seq);
    /**
     * \return the sequence number
     */
    uint32_t GetSeq() const;

    /**
     * \param jobId the job ID
     */
    void SetJobId(uint32_t jobId);
    /**
     * \return the job ID
     */
    uint32_t GetJobId() const;

    /**
     * \param size the size information
     */
    void SetSize(uint64_t size);
    /**
     * \return the size information
     */
    uint64_t GetSize() const;

  private:
    uint32_t m_seq;   //!< Sequence number
    uint32_t m_jobId; //!< Job ID
    uint64_t m_size;  //!< Size information
};

} // namespace ns3

#endif /* ATP_TAG_H */
