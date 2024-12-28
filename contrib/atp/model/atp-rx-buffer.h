#ifndef ATP_RX_BUFFER_H
#define ATP_RX_BUFFER_H

#include "ns3/object.h"
#include "ns3/sequence-number.h"
#include "ns3/packet.h"

#include <list>
#include <map>
#include <set>
#include <vector>

namespace ns3 {

/**
 * \brief ATP协议接收缓冲区的数据项
 */
class ATPRxItem
{
public:
  ATPRxItem()
    : m_packet(nullptr),
      m_startSeq(0),
      m_size(0)
  {}

  Ptr<Packet> m_packet;       //!< 数据包
  SequenceNumber32 m_startSeq; //!< 起始序号
  uint32_t m_size;            //!< 数据包大小
};

/**
 * \brief ATP协议接收缓冲区实现
 */
class ATPRxBuffer : public Object
{
public:
  static TypeId GetTypeId();
  
  ATPRxBuffer();
  ~ATPRxBuffer() override;

  /**
   * \brief 添加接收到的数据包到缓冲区
   * \param p 接收到的数据包
   * \param seq 数据包的序号
   * \return 是否添加成功
   */
  bool Add(Ptr<Packet> p, const SequenceNumber32& seq);

  /**
   * \brief 获取下一个连续的数据包
   * \return 按序到达的下一个数据包，如果没有则返回nullptr
   */
  Ptr<Packet> NextPacket();

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
   * \brief 获取下一个期望的序号
   * \return 期望收到的下一个序号
   */
  SequenceNumber32 NextExpectedSeq() const;

  /**
   * \brief 获取需要确认的序号列表
   * \return 需要发送ACK的序号列表
   */
  std::vector<SequenceNumber32> GetAckList();

private:
  typedef std::map<SequenceNumber32, ATPRxItem*> PacketMap; //!< 数据包映射表类型
  
  PacketMap m_rxMap;          //!< 接收数据映射表
  uint32_t m_maxBuffer;       //!< 最大缓冲区大小
  uint32_t m_size;           //!< 当前数据大小
  SequenceNumber32 m_nextSeq; //!< 期望的下一个序号

  // 记录需要确认的包
  std::set<SequenceNumber32> m_pendingAcks;  //!< 待确认的序号列表
};

} // namespace ns3

#endif /* ATP_RX_BUFFER_H */
