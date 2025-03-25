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
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"

namespace ns3
{

/**
 * \brief ATP packet header
 *
 * The header contains: 多少字节？
 * 1. 控制标志 -- m_packetType -- 1字节
 * 2. 任务ID -- m_jobId -- 1字节
 * 3. 序列号 -- m_seqNum -- 1字节
 * 4. 确认号 -- m_ackNum -- 1字节
 * 5. 窗口大小 -- m_windowSize -- 2字节
 * 6. 携带的梯度数据大小 -- m_size -- 2字节
 * 7. 源端口 -- m_sourcePort -- 2字节
 * 8. 目的端口 -- m_destinationPort -- 2字节
 *    总字节数 = 12字节
 * 9. 源地址 -- m_source -- (辅助作用，不计入)
 * 10. 目的地址 -- m_destination -- (辅助作用，不计入)
 * 11. 协议类型 -- m_protocol -- (辅助作用，不计入)
 */
class ATPHeader : public Header
{
  public:
    ATPHeader();

    // 定义数据包类型
    enum PacketType : uint8_t {
      DATA = 0x01,         // 普通数据包
      ACK = 0x02,          // 确认包
      AGG = 0x04,          // 聚合数据包
      ECN = 0x08,          // 拥塞通知  
      UNKNOWN = 0x00       // 未知类型
    };

    // 设置和获取数据包类型
    void SetPacketType(PacketType type);
    void ClearPacketType(PacketType type);
    uint8_t GetPacketType() const;

    // 任务ID
    void SetJobId(uint8_t jobId);
    uint8_t GetJobId() const;

    // 序列号
    void SetSeqNumber(uint32_t seqNum);
    uint8_t GetSeqNumber() const;

    // 确认号
    void SetAckNumber(uint8_t ackNum);
    uint8_t GetAckNumber() const;

    // 窗口大小，告诉发送方自己可以接收多少数据
    void SetWindowSize(uint16_t windowSize);
    uint16_t GetWindowSize() const;

    // 携带的梯度数据大小
    void SetSize(uint16_t size);
    uint16_t GetSize() const;

    // 设置和获取源端口和目的端口
    void SetSourcePort(uint16_t port);
    uint16_t GetSourcePort() const;
    void SetDestinationPort(uint16_t port);
    uint16_t GetDestinationPort() const;

    // 设置和获取源地址和目的地址
    void SetSourceAddress(const Address& source);
    Address GetSourceAddress() const;
    void SetDestinationAddress(const Address& destination);
    Address GetDestinationAddress() const;

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

  private:

    uint8_t m_packetType{UNKNOWN};      //!< 数据包类型
    uint8_t m_jobId{0};                 //!< 任务ID
    uint32_t m_seqNum{0};                //!< 序列号
    uint8_t m_ackNum{0};                //!< 确认号
    uint16_t m_size{0};                 //!< 数据大小
    uint16_t m_windowSize{0xffff};      //!< 接收窗口大小, 默认最大为65535

    uint16_t m_sourcePort{0xfffd};      //!< Source port
    uint16_t m_destinationPort{0xfffd}; //!< Destination port

    // 辅助字段
    Address m_source;                   //!< Source IP address
    Address m_destination;              //!< Destination IP address
    uint8_t m_protocol{142};
};

} // namespace ns3

#endif /* ATP_HEADER_H */
