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
    .AddConstructor<ATPTxBuffer>()
    .AddTraceSource("cwndTrace",
                    "Congestion window length",
                    MakeTraceSourceAccessor(&ATPTxBuffer::m_cwndTrace),
                    "ns3::TracedValueCallback::Uint32");
  return tid;
}

ATPTxBuffer::ATPTxBuffer()
  : m_bufferSize(0), // 需要socket进行设置，比较重要
    m_bufferDataSize(0),
    m_packetNum(0),
    m_virtualCwnd(1.0),
    m_cwnd(1)              //!< 拥塞窗口大小
{
  NS_LOG_FUNCTION(this);
}

ATPTxBuffer::~ATPTxBuffer()
{
    NS_LOG_FUNCTION(this);
    
    // 取消定时器
    if (m_windowIncreaseEvent.IsPending()) {
        m_windowIncreaseEvent.Cancel();
    }
    
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

    // 清理重传队列
    while (!m_retxQueue.empty())
    {
        ATPTxItem* item = m_retxQueue.front();
        m_retxQueue.pop();
        delete item;
    }
}

void
ATPTxBuffer::SetMaxBufferSize(uint32_t size)
{
    m_bufferSize = size;
    m_maxCwnd = size / 248;
}

void
ATPTxBuffer::SetCwnd(uint32_t cwnd)
{
    m_cwnd = cwnd;
    m_virtualCwnd = static_cast<double>(m_cwnd);
    m_cwndTrace = m_cwnd;
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

uint32_t
ATPTxBuffer::GetSentPacketId() const
{
    if (m_sentQueue.empty()) {
        NS_LOG_WARN("GetSentPacketId: m_sentQueue is empty");
        return 0;
    }

    return m_sentQueue.front()->m_packetId;
}

uint32_t
ATPTxBuffer::GetPendingFrontPacketId() const
{
    if (m_pendingQueue.empty()) {
        NS_LOG_WARN("GetPendingFrontPacketId: m_pendingQueue is empty");
        return 0;
    }

    return m_pendingQueue.front()->m_packetId;
}

uint32_t
ATPTxBuffer::GetCwndLeftBound() const
{
    return m_leftBound;
}

void
ATPTxBuffer::UpdateCwndLeftBound(uint32_t ackNum)
{
    NS_LOG_FUNCTION(this << ackNum);
    m_leftBound = ackNum;
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
ATPTxBuffer::IsOrderedAck(uint32_t packetId) const
{
    if (m_sentQueue.empty())
    {
        NS_LOG_WARN("IsOrderedAck: m_sentQueue is empty");
        return false;
    }

    ATPTxItem* item = m_sentQueue.front();
    uint32_t nextExpectedAckId = item->m_packetId;
    return packetId == nextExpectedAckId;
}

void
ATPTxBuffer::ProcessOrderedAck(uint32_t packetId)
{
    NS_LOG_FUNCTION(this << packetId);
    
    // 从m_sentQueue中移除已确认的包
    ATPTxItem* item = m_sentQueue.front();
    if (item->m_packetId == packetId)
    {
        m_sentQueue.pop();
        delete item;
    }
    else{  
        NS_LOG_WARN("Ordered ack, but packetId in m_sentQueue is not expected");
    }
}

Ptr<Packet>
ATPTxBuffer::ProcessUnorderedAck(uint32_t packetId)
{
    NS_LOG_FUNCTION(this << packetId);

    if (m_sentQueue.empty()) {
        NS_LOG_WARN("ProcessUnorderedAck: m_sentQueue is empty");
        return nullptr;
    }

    ATPTxItem* item = m_sentQueue.front();
    uint32_t nextExpectedAckId = item->m_packetId;

    if (packetId > nextExpectedAckId)
    {
        NS_LOG_WARN("Unordered ack received: " << packetId << ", expected: " << nextExpectedAckId);
        PacketQueue tempQueue;
        bool found = false;

        // 遍历队列找出所有需要重传的包
        while (!m_sentQueue.empty()) {
            ATPTxItem* front = m_sentQueue.front();
            m_sentQueue.pop();
            
            if (front->m_packetId == packetId) {
                // 找到匹配的包，删除它
                delete front;
                found = true;
            } else if (front->m_packetId < packetId) {
                // 创建新的重传项
                ATPTxItem* retxItem = new ATPTxItem();
                retxItem->m_packet = front->m_packet->Copy();
                retxItem->m_packetId = front->m_packetId;
                retxItem->m_lastSentTime = front->m_lastSentTime;
                m_retxQueue.push(retxItem);
            } else {
                // ID大于收到的ACK ID，保留
                tempQueue.push(front);
            }
        }

        // 将所有保留的包按原顺序放回m_sentQueue
        while (!tempQueue.empty()) {
            m_sentQueue.push(tempQueue.front());
            tempQueue.pop();
        }

        if (!found) {
            NS_LOG_WARN("ProcessUnorderedAck: Packet with ID " << packetId << " not found in sent queue");
        }
    }
    else{
        NS_LOG_WARN("Unordered ack, but packetId is less than nextExpectedAckId");
    }

    return item->m_packet;
}

void
ATPTxBuffer::ProcessCongestion(bool isEcn)
{
    NS_LOG_FUNCTION(this << isEcn);
    
    if (isEcn)
    {
        // 当发生拥塞时，如果当前窗口大于2，则减半；否则只减1
        if (m_virtualCwnd >= 2.0) {
            m_virtualCwnd = m_virtualCwnd / 2;
        } else {
            m_virtualCwnd = 1.0;
        }
        
        m_cwnd = static_cast<uint32_t>(m_virtualCwnd);
        if (m_cwnd > m_maxCwnd) {
            m_cwnd = m_maxCwnd;
        }
        m_cwndTrace = m_cwnd;
    }
    else{
        // Ensure floating-point division by using 1.0
        m_virtualCwnd += 1.0 / m_cwnd;
        m_cwnd = static_cast<uint32_t>(m_virtualCwnd);
        if (m_cwnd > m_maxCwnd) {
            m_cwnd = m_maxCwnd;
            // Ensure virtualCwnd doesn't exceed maxCwnd representation
            m_virtualCwnd = static_cast<double>(m_maxCwnd); 
        }
        m_cwndTrace = m_cwnd;
    }
}

bool
ATPTxBuffer::HasPacketToRetransmit() const
{
    return !m_retxQueue.empty();
}

Ptr<Packet>
ATPTxBuffer::RetransmitPacket()
{
    NS_LOG_FUNCTION(this);

    if (m_retxQueue.empty()) {
        NS_LOG_WARN("RetransmitPacket: m_retxQueue is empty");
        return nullptr;
    }

    ATPTxItem* item = m_retxQueue.front();
    m_retxQueue.pop();
    Ptr<Packet> packet = item->m_packet;
    delete item;

    return packet;
}

} // namespace ns3
