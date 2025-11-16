#ifndef ATP_TX_BUFFER_H
#define ATP_TX_BUFFER_H

#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/traced-value.h"

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
      m_lastSentTime(0)
  {}
  ~ATPTxItem()
  {
    m_packet = nullptr;
  }

    Ptr<Packet> m_packet;       //!< 数据包
    uint32_t m_packetId;        //!< 数据包ID
    uint32_t m_lastSentTime;    //!< 最后发送时间（微秒）
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
     * \brief 设置最大缓冲区大小
     * \param size 最大大小
     */
    void SetMaxBufferSize(uint32_t size);

    /**
     * \brief 设置拥塞窗口大小
     * \param cwnd 拥塞窗口大小
     */
    void SetCwnd(uint32_t cwnd);

    /**
     * \brief 获取拥塞窗口大小
     * \return 拥塞窗口大小
     */
    uint32_t GetCwnd() const;

    /**
     * \brief 添加数据到发送缓冲区
     * \param p 要添加的数据包
     * \return 是否添加成功
     */
    bool AddPacket(Ptr<Packet> p);

    /**
     * \brief 获取下一个要发送的数据包
     * \return 要发送的数据包，如果没有可发送的包则返回nullptr
     */
    Ptr<Packet> SendPacket();

    /**
     * \brief 检查ACK是否按序到达
     * \param packetId 收到的ACK包ID
     * \return 是否按序
     */
    bool IsOrderedAck(uint32_t packetId) const;

    /**
     * \brief 处理ACK
     * \param packetId 收到的ACK包ID
     * \return 是否按序
     */
    void ProcessOrderedAck(uint32_t packetId); 

    /**
     * \brief 处理乱序ACK
     * \param packetId 收到的ACK包ID
     * \return 是否按序
     */
    Ptr<Packet> ProcessUnorderedAck(uint32_t packetId);

    /**
     * \brief 处理拥塞状态
     * \param isEcn 是否拥塞
     */
    void ProcessCongestion(bool isEcn);

    /**
     * \brief 检查是否需要重传
     * \return 是否需要重传
     */
    bool HasPacketToRetransmit() const;

    /**
     * \brief 重传数据包
     */
    Ptr<Packet> RetransmitPacket();
    
    /**
     * \brief 检查超时的数据包并移到重传队列
     * \param timeoutUs 超时时间（微秒）
     * \return 超时的数据包数量
     */
    uint32_t CheckAndMoveTimeoutPackets(uint32_t timeoutUs);

    /**
     * \brief 获取已发送队列的第一个数据包ID
     * \return 数据包ID
     */
    uint32_t GetSentPacketId() const;

    /**
     * \brief 获取待发送队列的第一个数据包ID
     * \return 数据包ID
     */
    uint32_t GetPendingFrontPacketId() const;

    /**
     * \brief 获取拥塞窗口左边界
     * \return 拥塞窗口左边界
     */
    uint32_t GetCwndLeftBound() const;

    /**
     * \brief 更新拥塞窗口左边界
     * \param ackId 收到的ACK包ID
     */
    void UpdateCwndLeftBound(uint32_t ackNum);

    typedef std::queue<ATPTxItem*> PacketQueue; //!< 数据包队列类型
    
    PacketQueue m_pendingQueue;         //!< 待发送数据队列
    PacketQueue m_sentQueue;            //!< 已发送数据队列
    PacketQueue m_retxQueue;            //!< 重传数据队列
    uint32_t m_bufferSize;              //!< 最大缓冲区大小
    uint32_t m_bufferDataSize;          //!< 缓冲区内的数据大小
    uint32_t m_packetNum;               //!< 进入缓冲区的数据包总数量

    // 窗口管理
    uint32_t m_maxCwnd{1};              //!< 最大拥塞窗口长度
    uint32_t m_leftBound{0};            //!< 最小拥塞窗口左边界，左边界之前都是已经发送的数据包
    double m_virtualCwnd{1.0};          //!< 虚拟拥塞窗口长度
    uint32_t m_cwnd{1};                 //!< 当前拥塞窗口长度
    TracedValue<uint32_t> m_cwndTrace;  //!< 拥塞窗口长度

    EventId m_windowIncreaseEvent;  //!< 窗口增长定时器事件
};

} // namespace ns3

#endif /* ATP_TX_BUFFER_H */
