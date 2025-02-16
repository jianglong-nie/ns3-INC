#include "atp-tx-buffer.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ATPTxBuffer");
NS_OBJECT_ENSURE_REGISTERED(ATPTxBuffer);

TypeId
ATPTxBuffer::GetTypeId()
{
  static TypeId tid = TypeId("ns3::ATPTxBuffer")
    .SetParent<Object>()
    .SetGroupName("Internet")
    .AddConstructor<ATPTxBuffer>();
  return tid;
}

ATPTxBuffer::ATPTxBuffer()
  : m_bufferSize(0), // 需要socket进行设置，比较重要
    m_bufferDataSize(0),
    m_packetNum(0),
    m_cwnd(2),              //!< 拥塞窗口大小
    m_nextExpectedAckId(1)    //!< 期望收到的下一个ACK的ID
{
  NS_LOG_FUNCTION(this);
}

ATPTxBuffer::~ATPTxBuffer()
{
    NS_LOG_FUNCTION(this);
    
    // 清理待发送队列
    while (!m_pendingQueue.empty())
    {
        ATPTxItem* item = m_pendingQueue.front();
        m_pendingQueue.pop();
        delete item;
    }
    
    // 清理已发送队列
    while (!m_sentQueue.empty())
    {
        ATPTxItem* item = m_sentQueue.front();
        m_sentQueue.pop();
        delete item;
    }
}

void
ATPTxBuffer::SetMaxBufferSize(uint32_t size)
{
    m_bufferSize = size;
}

void
ATPTxBuffer::SetCwnd(uint32_t cwnd)
{
    m_cwnd = cwnd;
}

uint32_t
ATPTxBuffer::GetCwnd() const
{
    return m_cwnd;
}

bool
ATPTxBuffer::AddPacket(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);
    
    if (p->GetSize() + m_bufferDataSize > m_bufferSize)
    {
        NS_LOG_WARN("Buffer full, packet dropped");
        return false;
    }
    
    // 添加数据包到缓冲区
    auto item = new ATPTxItem();
    item->m_packet = p->Copy();
    m_packetNum++; // 数据包序号
    item->m_packetId = m_packetNum;
    m_pendingQueue.push(item);
    m_bufferDataSize += p->GetSize();
    
    return true;
}

Ptr<Packet>
ATPTxBuffer::SendPacket()
{
    NS_LOG_FUNCTION(this);

    // 检查是否有可用窗口
    if (m_pendingQueue.empty()) {
        NS_LOG_WARN("Buffer is empty, no packet to send");
        return nullptr;
    }

    // 发送新的数据包
    ATPTxItem* item = m_pendingQueue.front();
    m_pendingQueue.pop();
    m_bufferDataSize -= item->m_packet->GetSize();

    // 更新发送时间，将item存入已发送队列
    item->m_lastSentTime = static_cast<uint32_t>(Simulator::Now().GetMilliSeconds());
    m_sentQueue.push(item);

    return item->m_packet;
}

bool
ATPTxBuffer::ProcessAck(uint32_t packetId)
{
    NS_LOG_FUNCTION(this << packetId);
    
    // 先判断是否按序到达，根据情况处理拥塞窗口
    // 如果按序，从m_sentQueue中移除已确认的包，否则重传所有的包
    if (IsOrderedAck(packetId)) {
        UpdateWindow(true);
        // 从m_sentQueue中移除已确认的包
        while (!m_sentQueue.empty())
        {
            ATPTxItem* item = m_sentQueue.front();
            if (item->m_packetId <= packetId)
            {
                m_sentQueue.pop();
                delete item;
            }
            else
            {
                break;
            }
        }
        return true;
    }
    else {
        NS_LOG_INFO("Unordered ack, update window");
        UpdateWindow(false);
        return false;
    }
}

bool
ATPTxBuffer::IsOrderedAck(uint32_t packetId) const
{
    return packetId == m_nextExpectedAckId;
}

void
ATPTxBuffer::UpdateWindow(bool isOrdered)
{
    if (isOrdered) {
        // 按序到达，窗口长度增加1，更新窗口左边界
        m_cwnd++;
        m_nextExpectedAckId++;
    } else {
        // 乱序到达，窗口长度减半，窗口左边界不变，开始重传数据包
        m_cwnd = std::max(static_cast<uint32_t>(2), m_cwnd / 2);
    }
    NS_LOG_INFO("Update window: cwnd = " << m_cwnd 
                << ", ordered = " << isOrdered);
}

} // namespace ns3
