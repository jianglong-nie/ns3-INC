#ifndef ATP_TX_BUFFER_H
#define ATP_TX_BUFFER_H

#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <queue>
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
      m_packetId(0),
      m_acked(false),
      m_lastSentTime(0)
  {}

    Ptr<Packet> m_packet;       //!< 数据包
    uint32_t m_packetId;        //!< 数据包ID
    bool m_acked;               //!< 是否已确认
    uint32_t m_lastSentTime;    //!< 最后发送时间
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
    void Ack(uint32_t packetId); 

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

  private:
    typedef std::queue<ATPTxItem*> PacketQueue; //!< 数据包队列类型
    
    PacketQueue m_pendingQueue;         //!< 待发送数据队列
    PacketQueue m_sentQueue;            //!< 已发送数据队列
    uint32_t m_maxBufferSize;           //!< 最大缓冲区大小
    uint32_t m_size;                    //!< 当前缓冲区数据大小
    uint32_t m_sentSize;                //!< 已发送数据大小
    uint32_t m_packetNum;               //!< 进入缓冲区的数据包总数量
};

} // namespace ns3

#endif /* ATP_TX_BUFFER_H */
