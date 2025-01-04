#ifndef ATP_CONGESTION_CONTROL_H
#define ATP_CONGESTION_CONTROL_H

#include "ns3/tcp-congestion-ops.h"

namespace ns3 {

/**
 * \brief ATP拥塞控制算法的实现
 *
 * 特点:
 * - 有初始发送窗口大小
 * - 每个ACK使窗口+1，直到达到阈值
 * - 收到ECN时窗口减半，并更新慢启动阈值
 */
class ATPCC : public TcpCongestionOps
{
public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    ATPCC();

    /**
     * \brief Copy constructor
     * \param sock object to copy
     */
    ATPCC(const ATPCC& sock);

    ~ATPCC() override;

    std::string GetName() const override;

    void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
    Ptr<TcpCongestionOps> Fork() override;
    
    // 处理ECN信号
    void CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event) override;

private:
    uint32_t m_initialWindow;  // 初始窗口大小
    uint32_t m_maxWindow;      // 最大窗口阈值
};

} // namespace ns3

#endif // ATP_CONGESTION_CONTROL_H
