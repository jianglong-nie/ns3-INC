#ifndef ATP_TX_BUFFER_H
#define ATP_TX_BUFFER_H

#include "ns3/object.h"
#include "ns3/sequence-number.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <list>
#include <vector>

namespace ns3 {

/**
 * \brief ATP协议发送缓冲区的数据项
 */
class ATPTxItem
{
public:
  ATPTxItem() 
    : m_packet(nullptr),
      m_startSeq(0),
      m_lastSent(0),
      m_retransCount(0),
      m_acked(false)
  {}

  Ptr<Packet> m_packet;       //!< 数据包
  SequenceNumber32 m_startSeq; //!< 起始序号
  uint32_t m_lastSent;        //!< 最后发送时间(ms)
  uint32_t m_retransCount;    //!< 重传次数
  bool m_acked;               //!< 是否已确认
};

/**
 * \brief ATP协议发送缓冲区实现
 */
class ATPTxBuffer : public Object
{
public:
  static TypeId GetTypeId();
  
  ATPTxBuffer();
  ~ATPTxBuffer() override;

  /**
   * \brief 添加数据到发送缓冲区
   * \param p 要添加的数据包
   * \return 是否添加成功
   */
  bool Add(Ptr<Packet> p);

  /**
   * \brief 确认特定序号的数据包已收到
   * \param startSeq 要确认的包的起始序号
   */
  void Ack(const SequenceNumber32& startSeq); 

  /**
   * \brief 获取缓冲区大小
   * \return 缓冲区当前大小
   */
  uint32_t Size() const;

  /**
   * \brief 获取可用空间
   * \return 可用空间大小
   */
  uint32_t Available() const;

  /**
   * \brief 检查缓冲区是否为空
   * \return 是否为空
   */
  bool IsEmpty() const;

  /**
   * \brief 设置最大缓冲区大小
   * \param size 最大大小
   */
  void SetMaxBufferSize(uint32_t size);

  /**
   * \brief 获取下一个要发送的数据包
   * \return 要发送的数据包，如果没有可发送的包则返回nullptr
   */
  Ptr<Packet> NextPacket();

  /**
   * \brief 获取需要重传的数据包
   * \param currentTime 当前时间(ms)
   * \param rto 重传超时时间(ms)
   * \param maxRetrans 最大重传次数
   * \return 需要重传的数据包列表
   */
  std::vector<Ptr<Packet>> GetRetransmitPackets(uint32_t currentTime, 
                                               uint32_t rto,
                                               uint32_t maxRetrans = 5);

private:
  typedef std::list<ATPTxItem*> PacketList; //!< 数据包列表类型
  
  PacketList m_pendingList;   //!< 待发送数据列表
  PacketList m_sentList;      //!< 已发送数据列表
  
  uint32_t m_maxBuffer;       //!< 最大缓冲区大小
  uint32_t m_size;           //!< 当前数据大小
  uint32_t m_sentSize;       //!< 已发送数据大小
  
  SequenceNumber32 m_nextSeq; //!< 下一个序号
};

} // namespace ns3

#endif /* ATP_TX_BUFFER_H */
