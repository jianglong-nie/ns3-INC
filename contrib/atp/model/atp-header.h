/*
 * Copyright (c) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 */

#ifndef ATP_HEADER_H
#define ATP_HEADER_H

#include "ns3/header.h"
#include "ns3/nstime.h"

namespace ns3
{

/**
 * \brief ATP packet header to carry sequence number, job ID, timestamp and size
 *
 * The header contains a 32 bit sequence number, a 32 bit job ID,
 * a 64 bit timestamp, and a 64 bit size field (24 bytes total).
 */
class ATPHeader : public Header
{
  public:
    ATPHeader();

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
     * \return the timestamp
     */
    Time GetTs() const;

    /**
     * \param size the size information
     */
    void SetSize(uint64_t size);
    /**
     * \return the size information
     */
    uint64_t GetSize() const;

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    uint32_t m_seq;   //!< Sequence number
    uint32_t m_jobId; //!< Job ID
    uint64_t m_ts;    //!< Timestamp
    uint64_t m_size;  //!< Size information
};

} // namespace ns3

#endif /* ATP_HEADER_H */
