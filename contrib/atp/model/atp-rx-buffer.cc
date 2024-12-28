#include "atp-rx-buffer.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ATPRxBuffer");
NS_OBJECT_ENSURE_REGISTERED(ATPRxBuffer);

TypeId
ATPRxBuffer::GetTypeId()
{
  static TypeId tid = TypeId("ns3::ATPRxBuffer")
    .SetParent<Object>()
    .SetGroupName("Internet")
    .AddConstructor<ATPRxBuffer>()
    ;
  return tid;
}

ATPRxBuffer::ATPRxBuffer()
  : m_maxBuffer(32768), // 默认32KB
    m_size(0),
    m_nextSeq(0)
{
  NS_LOG_FUNCTION(this);
}

ATPRxBuffer::~ATPRxBuffer()
{
  NS_LOG_FUNCTION(this);
  
  // 清理接收缓冲区
  for (auto it = m_rxMap.begin(); it != m_rxMap.end(); ++it)
  {
    delete it->second;
  }
  m_rxMap.clear();
}

bool
ATPRxBuffer::Add(Ptr<Packet> p, const SequenceNumber32& seq)
{
  NS_LOG_FUNCTION(this << p << seq);
  
  // 检查缓冲区空间
  if (p->GetSize() + m_size > m_maxBuffer)
  {
    NS_LOG_WARN("Buffer full, packet dropped");
    return false;
  }

  // 检查是否是重复的包
  auto it = m_rxMap.find(seq);
  if (it != m_rxMap.end())
  {
    NS_LOG_INFO("Duplicate packet received, seq=" << seq);
    // 即使是重复的包也要发送ACK，因为可能之前的ACK丢失了
    m_pendingAcks.insert(seq);
    return false;
  }
  
  // 创建新的接收项
  auto item = new ATPRxItem();
  item->m_packet = p->Copy();
  item->m_startSeq = seq;
  item->m_size = p->GetSize();
  
  // 添加到接收映射表
  m_rxMap[seq] = item;
  m_size += p->GetSize();
  
  // 将序号加入待确认列表
  m_pendingAcks.insert(seq);
  
  return true;
}

Ptr<Packet>
ATPRxBuffer::NextPacket()
{
  NS_LOG_FUNCTION(this);
  
  if (m_rxMap.empty())
  {
    return nullptr;
  }
  
  // 检查是否有按序到达的包
  auto it = m_rxMap.find(m_nextSeq);
  if (it != m_rxMap.end())
  {
    ATPRxItem* item = it->second;
    Ptr<Packet> p = item->m_packet;
    
    // 更新下一个期望序号
    m_nextSeq += item->m_size;
    
    // 从缓冲区移除
    m_size -= item->m_size;
    m_rxMap.erase(it);
    delete item;
    
    return p;
  }
  
  // 如果当前没有按序到达的包，则返回第一个包
  return m_rxMap.begin()->second->m_packet;
}

uint32_t
ATPRxBuffer::Size() const
{
  return m_size;
}

uint32_t
ATPRxBuffer::Available() const
{
  return m_maxBuffer - m_size;
}

bool
ATPRxBuffer::IsEmpty() const
{
  return m_size == 0;
}

void
ATPRxBuffer::SetMaxBufferSize(uint32_t size)
{
  m_maxBuffer = size;
}

SequenceNumber32
ATPRxBuffer::NextExpectedSeq() const
{
  return m_nextSeq;
}

std::vector<SequenceNumber32>
ATPRxBuffer::GetAckList()
{
  std::vector<SequenceNumber32> acks(m_pendingAcks.begin(), m_pendingAcks.end());
  m_pendingAcks.clear();  // 清空待确认列表
  return acks;
}

} // namespace ns3
