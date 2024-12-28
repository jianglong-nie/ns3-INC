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
    ;
  return tid;
}

ATPTxBuffer::ATPTxBuffer()
  : m_maxBuffer(32768), // 默认32KB
    m_size(0),
    m_sentSize(0),
    m_nextSeq(0)
{
  NS_LOG_FUNCTION(this);
}

ATPTxBuffer::~ATPTxBuffer()
{
  NS_LOG_FUNCTION(this);
  
  // 清理待发送列表
  for (auto it = m_pendingList.begin(); it != m_pendingList.end(); ++it)
  {
    delete *it;
  }
  m_pendingList.clear();
  
  // 清理已发送列表
  for (auto it = m_sentList.begin(); it != m_sentList.end(); ++it)
  {
    delete *it;
  }
  m_sentList.clear();
}

bool
ATPTxBuffer::Add(Ptr<Packet> p)
{
  NS_LOG_FUNCTION(this << p);
  
  if (p->GetSize() + m_size > m_maxBuffer)
  {
    NS_LOG_WARN("Buffer full, packet dropped");
    return false;
  }
  
  auto item = new ATPTxItem();
  item->m_packet = p->Copy();
  item->m_startSeq = m_nextSeq;
  m_nextSeq += p->GetSize();

  m_pendingList.push_back(item);
  m_size += p->GetSize();
  
  return true;
}

Ptr<Packet>
ATPTxBuffer::NextPacket()
{
  NS_LOG_FUNCTION(this);
  
  uint32_t currentTime = static_cast<uint32_t>(Simulator::Now().GetMilliSeconds());
  
  // 最后发送新的包
  if (!m_pendingList.empty())
  {
    ATPTxItem* item = m_pendingList.front();
    m_pendingList.pop_front();
    item->m_lastSent = currentTime;
    m_sentList.push_back(item);
    m_sentSize += item->m_packet->GetSize();
    
    return item->m_packet;
  }
  
  return nullptr;
}

void
ATPTxBuffer::Ack(const SequenceNumber32& startSeq)
{
  NS_LOG_FUNCTION(this << startSeq);
  
  auto it = m_sentList.begin();
  while (it != m_sentList.end())
  {
    ATPTxItem* item = *it;
    
    // 直接匹配包的起始序号
    if (item->m_startSeq == startSeq)
    {
      // 标记为已确认
      item->m_acked = true;
      
      // 释放已确认的包
      m_sentSize -= item->m_packet->GetSize();
      m_size -= item->m_packet->GetSize();
      
      it = m_sentList.erase(it);
      delete item;
      return;  // 找到并处理了对应的包，可以直接返回
    }
    else
    {
      ++it;
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
  return m_maxBuffer - m_size;
}

bool
ATPTxBuffer::IsEmpty() const
{
  return m_size == 0;
}

void
ATPTxBuffer::SetMaxBufferSize(uint32_t size)
{
  m_maxBuffer = size;
}

std::vector<Ptr<Packet>>
ATPTxBuffer::GetRetransmitPackets(uint32_t currentTime, uint32_t rto, uint32_t maxRetrans)
{
  NS_LOG_FUNCTION(this << currentTime << rto);
  
  std::vector<Ptr<Packet>> packets;
  
  for (auto it = m_sentList.begin(); it != m_sentList.end(); ++it)
  {
    ATPTxItem* item = *it;
    
    // 检查未确认的包是否需要重传
    if (!item->m_acked && 
        (currentTime - item->m_lastSent) >= rto && 
        item->m_retransCount < maxRetrans)
    {
      item->m_retransCount++;
      item->m_lastSent = currentTime;
      packets.push_back(item->m_packet);
      
      NS_LOG_INFO("Packet with seq=" << item->m_startSeq 
                 << " needs retransmission, count=" << item->m_retransCount);
    }
  }
  
  return packets;
}

} // namespace ns3
