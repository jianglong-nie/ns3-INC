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
  : m_maxBufferSize(32768), // 默认32KB
    m_size(0),
    m_sentSize(0),
    m_packetNum(0)
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

bool
ATPTxBuffer::Add(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);
    
    if (p->GetSize() + m_size > m_maxBufferSize)
    {
        NS_LOG_WARN("Buffer full, packet dropped");
        return false;
    }
    
    auto item = new ATPTxItem();
    item->m_packet = p->Copy();
    m_packetNum++;
    item->m_packetId = m_packetNum;

    m_pendingQueue.push(item);
    m_size += p->GetSize();
    
    return true;
}

Ptr<Packet>
ATPTxBuffer::NextPacket()
{
    NS_LOG_FUNCTION(this);

    if (m_pendingQueue.empty())
    {
        return nullptr;
    }
    else
    {
        uint32_t currentTime = static_cast<uint32_t>(Simulator::Now().GetMilliSeconds());
        
        // 最后发送新的包
        ATPTxItem* item = m_pendingQueue.front();
        m_pendingQueue.pop();
        m_size -= item->m_packet->GetSize();

        // 更新发送时间，将item存入已发送队列
        item->m_lastSentTime = currentTime;
        m_sentQueue.push(item);
        m_sentSize += item->m_packet->GetSize();
        
        return item->m_packet;
    }
}

void
ATPTxBuffer::Ack(uint32_t packetId)
{
    NS_LOG_FUNCTION(this << packetId);
    
    // 从已发送队列中移除已确认的包
    // 如果已确认的包在队列中，则移除该包
    // 对于队列中小于已确认packetId的包，则将这些包同样pop
    while (!m_sentQueue.empty())
    {
        ATPTxItem* item = m_sentQueue.front();
        if (item->m_packetId == packetId)
        {
            m_sentQueue.pop();
            m_sentSize -= item->m_packet->GetSize();
            delete item;
            break;
        }
        else
        {
            // 默认m_sentQueue按packetId顺序排序
            // 因为考虑到NextPacket()是按packetId顺序存入m_sentQueue的
            m_sentQueue.pop();
            m_sentSize -= item->m_packet->GetSize();
            delete item;
        }
    }
}

uint32_t
ATPTxBuffer::Size() const
{
  return m_size;
}

uint32_t 
ATPTxBuffer::Available() const
{
  return m_maxBufferSize - m_size;
}

bool
ATPTxBuffer::IsEmpty() const
{
  return m_size == 0;
}

void
ATPTxBuffer::SetMaxBufferSize(uint32_t size)
{
  m_maxBufferSize = size;
}

} // namespace ns3
