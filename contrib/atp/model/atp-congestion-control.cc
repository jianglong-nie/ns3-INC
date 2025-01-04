#include "atp-congestion-control.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("ATPCC");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(ATPCC);

TypeId
ATPCC::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ATPCC")
                           .SetParent<TcpCongestionOps>()
                           .SetGroupName("Internet")
                           .AddConstructor<ATPCC>();
    return tid;
}

ATPCC::ATPCC()
    : TcpCongestionOps()
{
    NS_LOG_FUNCTION(this);
    // 设置初始值
    m_initialWindow = 10;  // 10个MSS
    m_maxWindow = 100;     // 100个MSS
}

ATPCC::ATPCC(const ATPCC& sock)
    : TcpCongestionOps(sock)
{
    NS_LOG_FUNCTION(this);
    m_initialWindow = sock.m_initialWindow;
    m_maxWindow = sock.m_maxWindow;
}

ATPCC::~ATPCC()
{
    NS_LOG_FUNCTION(this);
}

std::string
ATPCC::GetName() const
{
    return "ATPCC";
}

void
ATPCC::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (segmentsAcked > 0)
    {
        // 每收到一个ACK，窗口增加一个MSS
        uint32_t newWindow = tcb->m_cWnd + tcb->m_segmentSize;
        
        // 确保不超过最大窗口限制
        if (newWindow <= m_maxWindow * tcb->m_segmentSize)
        {
            tcb->m_cWnd = newWindow;
        }
        
        NS_LOG_INFO("Updated cwnd to " << tcb->m_cWnd << " bytes");
    }
}

uint32_t
ATPCC::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << tcb << bytesInFlight);
    
    // 返回当前飞行中字节数的一半，但不小于2个MSS
    return std::max(2 * tcb->m_segmentSize, bytesInFlight / 2);
}

void
ATPCC::CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
    NS_LOG_FUNCTION(this << tcb << event);

    if (event == TcpSocketState::CA_EVENT_ECN_IS_CE)
    {
        // 收到ECN时，将窗口减半
        uint32_t newWindow = tcb->m_cWnd / 2;
        
        // 确保窗口不小于初始窗口
        newWindow = std::max(newWindow, m_initialWindow * tcb->m_segmentSize);
        
        // 更新窗口和慢启动阈值
        tcb->m_ssThresh = newWindow;
        tcb->m_cWnd = newWindow;
        
        NS_LOG_INFO("ECN received: Updated cwnd to " << tcb->m_cWnd << " bytes");
    }
}

Ptr<TcpCongestionOps>
ATPCC::Fork()
{
    return CopyObject<ATPCC>(this);
}

} // namespace ns3
