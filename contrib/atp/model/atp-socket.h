#ifndef ATP_SOCKET_H
#define ATP_SOCKET_H

#include "atp-tag.h"
#include "atp-tx-buffer.h"
#include "atp-congestion-control.h"

#include "ns3/socket.h"
#include "ns3/traced-callback.h"
#include "ns3/callback.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-address.h"

#include "ns3/ptr.h"
#include "ns3/timer.h"
#include "ns3/data-rate.h"
#include "ns3/node.h"
#include "ns3/sequence-number.h"
#include "ns3/traced-value.h"

#include <queue>
#include <stdint.h>
#include <map>
#include <vector>

namespace ns3 {

class Node;
class Packet;
class ATPTag;
class ATPRxBuffer;
class ATPL4Protocol;
class Ipv4EndPoint;

/**
 * \brief ATP Socket实现
 * 
 * ATP是一个无序传输但具有拥塞控制和重传机制的协议
 */
class ATPSocket : public Socket
{
  public:
    static TypeId GetTypeId();
    
    ATPSocket();
    ~ATPSocket() override;

    // 需要实现的纯虚函数
    void SetNode(Ptr<Node> node);
    Ptr<Node> GetNode() const override;

    void SetATP(Ptr<ATPL4Protocol> atp);
    Ptr<ATPL4Protocol> GetATP() const;

    void SetRxBufferSize(uint32_t size);
    uint32_t GetRxBufferSize() const;

    uint32_t GetRxAvailable() const override;
    uint32_t GetTxAvailable() const override;

    SocketErrno GetErrno() const override;
    SocketType GetSocketType() const override;

    int GetSockName(Address& address) const override;
    int GetPeerName(Address& address) const override;

    bool SetAllowBroadcast(bool allowBroadcast) override;
    bool GetAllowBroadcast() const override;


    // 发送数据到下层，会调用DoSend
    int Send(Ptr<Packet> p, uint32_t flags) override;
    int SendTo(Ptr<Packet> p, uint32_t flags, const Address& address) override;
    
    
    // 给应用层设置的回调函数，用于应用层接收数据
    Ptr<Packet> Recv(uint32_t maxSize, uint32_t flags) override;
    Ptr<Packet> RecvFrom(uint32_t maxSize, uint32_t flags, Address& fromAddress) override;
    
    // 绑定和连接
    int FinishBind();
    int Bind() override;  // 绑定任意地址
    int Bind(const Address& address) override;  // 绑定地址
    int Bind6() override;  // 绑定IPv6地址
    void BindToNetDevice(Ptr<NetDevice> netdevice) override; // 绑定到网络设备
    
    int Connect(const Address& address) override;  // 连接对端
    int Close() override;  // 关闭连接
    int Listen() override; // 监听
    int ShutdownSend() override;  
    int ShutdownRecv() override;

    void Destroy();
    void DeallocateEndPoint();
    void CancelAllTimers();

    /**
     * \brief Get the transmission buffer
     * \return pointer to the transmission buffer
     */
    Ptr<ATPTxBuffer> GetTxBuffer() const { return m_txBuffer; }

    // 设置job 与 节点 <地址, 端口> 的映射
    void AddAddressMapping(uint8_t jobId, const Ipv4Address& addr, uint16_t port);
    const std::vector<std::pair<Ipv4Address, uint16_t>>& GetAddressMapping(uint8_t jobId) const;

  protected:
    void SendWindowData();
    int DoSend(Ptr<Packet> p);
    int DoSendTo(Ptr<Packet> p, Ipv4Address dest, uint16_t port);
    /* 
    L4层收到数据 -> ForwardUp -> rxCallback -> Recv/RecvFrom
    网络 -> ForwardUp -> 接收队列 -> RecvFrom -> 应用层 

    网络层(IP) → 传输层(UDP) → Socket → 应用层(PacketSink)
     ↓              ↓           ↓            ↓
    接收IP包     ForwardUp()   存入队列     HandleRead()
             检查有效性    管理缓冲区    处理应用数据
             拆除IP头     通知应用层
    */
    void ForwardUp(Ptr<Packet> p, Ipv4Header header, uint16_t sport, Ptr<Ipv4Interface> incomingInterface);

    // 处理ack包
    void ReceiveAck(ATPTag atpTag);
    void SendAck(ATPTag atpTag, Ipv4Header ipHeader);
    void SendMultiAck(const ATPTag& atpTag, const Ipv4Header& ipHeader);

    // 重传数据包
    void Retransmit();

    // 连接到ATP/IP的其它层
    Ipv4EndPoint* m_endPoint;          // 本地端点
    Ptr<Node> m_node;                  // 所属节点
    Ptr<ATPL4Protocol> m_atp;          // ATP协议实例

    // socket connect属性
    mutable SocketErrno m_errno; //!< Socket错误码
    bool m_shutdownSend;         //!< 发送不再允许
    bool m_shutdownRecv;         //!< 接收不再允许
    bool m_connected;            //!< 连接已建立

    // 地址
    Address m_defaultAddress;                      //!< 默认目标address
    uint16_t m_defaultPort;                        //!< 默认目标端口
    TracedCallback<Ptr<const Packet>> m_dropTrace; //!< 丢包跟踪
    
    EventId m_sendWindowDataEvent{};               //!< micro-delay event to send pending data
    EventId m_retxEvent{};                          //!< Retransmission event

    // 接收发送缓冲区
    bool m_allowBroadcast{false};                   // 是否允许广播
    uint32_t m_rxBufferSize;                        // 接收缓冲区大小
    uint32_t m_txBufferSize;                        // 发送缓冲区大小
    uint32_t m_rxAvailable;                         // 接收缓冲区可接收数据量
    uint32_t m_txAvailable;                         // 发送缓冲区可发送数据量
    Ptr<ATPTxBuffer> m_txBuffer;                    // 发送缓冲区，自定义的类型
    std::queue<std::pair<Ptr<Packet>, Address>> m_rxBuffer; // 接收缓冲区，是个队列
    
    // 拥塞控制
    Ptr<ATPCC> m_congestionControl;    // 拥塞控制算法
    uint32_t m_nextSeqNo;              // 下一个序列号
    uint32_t m_highestRxSeqNo;         // 最高接收序列号
    uint32_t m_initCwnd;               // 初始拥塞窗口
    uint32_t m_ssthresh;               // 慢启动阈值
    uint32_t m_mss;                    // 最大报文段大小
    
    // Ipv4EndPoint* m_endPoint;          // 本地端点
    // Address m_peerAddress;             // 对端地址
    TracedCallback<Ptr<const Packet>> m_txTrace;  // 发送跟踪
    TracedCallback<Ptr<const Packet>> m_rxTrace;  // 接收跟踪

    // 存储已知节点的IP地址和端口映射
    std::map<uint8_t, std::vector<std::pair<Ipv4Address, uint16_t>>> m_jobAddressMap;
};

} // namespace ns3

#endif /* ATP_SOCKET_H */
