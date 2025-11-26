#ifndef ATP_SOCKET_H
#define ATP_SOCKET_H

#include "atp-tag.h"
#include "atp-tx-buffer.h"
#include "atp-aggregator.h"

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
#include "ns3/nstime.h"

#include <queue>
#include <stdint.h>
#include <map>
#include <vector>
#include <unordered_map>
#include <boost/functional/hash.hpp> 

namespace ns3 {

class Node;
class Packet;
class ATPTag;
class ATPRxBuffer;
class ATPL4Protocol;
class Ipv4EndPoint;

struct PairHash {
    std::size_t operator()(const std::pair<uint8_t, uint32_t>& p) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, p.first);
        boost::hash_combine(seed, p.second);
        return seed;
    }
};

/**
 * \brief 描述单个聚合分支的结构
 * 
 * 每个分支对应 bitmap1 中的一位，代表一个边缘交换机（第一层聚合点）
 */
struct AggregationBranch
{
    uint32_t branchId;          //!< 分支ID（对应bitmap1中的位位置）
    uint8_t fanInDegree0;       //!< 该分支下第一层的扇入度（有多少个worker）
    uint32_t fullBitmap0;       //!< 该分支完全聚合时的bitmap0值（例如：3个worker则为0b111）
    
    AggregationBranch(uint32_t id = 0, uint8_t fanIn = 0, uint32_t bitmap = 0)
        : branchId(id), fanInDegree0(fanIn), fullBitmap0(bitmap) {}
};

/**
 * \brief 描述作业的完整聚合树结构
 * 
 * 用于参数服务器端理解任务的层次化拓扑，以便正确计算聚合状态
 */
struct JobTree
{
    uint8_t jobId;                              //!< 作业ID
    uint8_t fanInDegree1;                       //!< 第二层（核心层）的扇入度
    uint32_t fullBitmap1;                           //!< 第二层（核心层）的bitmap1值
    std::map<uint32_t, AggregationBranch> branches;  //!< 分支映射表：bitmap1位位置 -> 分支信息
    uint32_t expectedFlatBitmap;                //!< 期望的完整flat_bitmap（所有worker到达）
    
    JobTree(uint8_t jid = 0) : jobId(jid), fanInDegree1(0), fullBitmap1(0), expectedFlatBitmap(0) {}
    
    /**
     * \brief 添加一个聚合分支
     * \param bitmap1Pos bitmap1中的位位置（0-31）
     * \param fanIn0 该分支的第一层扇入度
     * \param fullBitmap0 该分支完全聚合时的bitmap0
     */
    void AddBranch(uint32_t bitmap1Pos, uint8_t fanIn0, uint32_t fullBitmap0)
    {
        branches[bitmap1Pos] = AggregationBranch(bitmap1Pos, fanIn0, fullBitmap0);
    }
    
    /**
     * \brief 根据JobTree结构计算flat_bitmap
     * \param bitmap0 当前的bitmap0
     * \param bitmap1 当前的bitmap1
     * \return 对应的flat_bitmap
     */
    // JobTree 内部
    uint32_t ConvertToFlatBitmap(uint32_t bitmap0, uint32_t bitmap1) const
    {
        uint32_t flat_bitmap   = 0;
        uint32_t worker_offset = 0;  // 当前分支在 flat_bitmap 中的起始 bit 位置

        // 存在多种情况
        // 1. 完全聚合，bitmap1里对应分支为都为1,bitmap0为随机值无所谓（=最后一个到第二层执行聚合的包里的bitmap0值
        //    返回 expectedFlatBitmap
        // 2. 1层聚合，bitmap1中一个分支为1，该分支下bitmap0的所有worker位都为1
        // 3. 没有聚合，bitmap1中一个分支为1，该分支下bitmap0的一个worker位为1
        // 4. 重传包导致的局部聚合，bitmap1中一个分支为1，该分支下bitmap0多个worker位为1

        if (bitmap1 == fullBitmap1)
        {
            return expectedFlatBitmap;
        }
        else
        {
            for (uint32_t b1_pos = 0; b1_pos < 32; ++b1_pos)
            {
                auto it = branches.find(b1_pos);
                if (it == branches.end())
                {
                    continue;
                }
                const AggregationBranch& branch = it->second;

                if (bitmap1 & (1u << b1_pos))
                {
                    flat_bitmap |= (bitmap0 << worker_offset);
                }
                worker_offset += branch.fanInDegree0;
                
            }
        }

        return flat_bitmap;
    }
  
    /**
     * \brief 检查聚合是否完成
     * \param flat_bitmap 当前的flat_bitmap
     * \return 是否所有worker都已到达
     */
    bool IsAggregationComplete(uint32_t flat_bitmap) const
    {
        return flat_bitmap == expectedFlatBitmap;
    }
};

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

    // 设置初始拥塞窗口
    void SetInitCwnd(uint32_t initCwnd);
    
    // 设置重传超时时间
    void SetRetxTimeout(Time timeout);
    
    // 设置超时检查间隔
    void SetRetxCheckInterval(Time interval);

    void SetJobBitmap(uint8_t jobId, uint32_t bitmap0, uint32_t bitmap1);
    
    uint32_t GetFlatBitmap(uint8_t jobId, uint32_t bitmap0, uint32_t bitmap1);

    // 添加bitmap映射表：手动配置(bitmap0, bitmap1) -> flat_bitmap的映射
    void AddFlatBitmapMapping(uint8_t jobId, uint32_t bitmap0, uint32_t bitmap1, uint32_t flat_bitmap);
    
    // 直接设置期望的完整 flat_bitmap（用于判断聚合完成）
    void SetExpectedAggFlatBitmap(uint8_t jobId, uint32_t expected_flat_bitmap);

    uint64_t GetTotalTxBytes() const { return m_totalTxBytes; }

    void SetJobTree(const JobTree& jobTree);
    JobTree& GetJobTree(uint8_t jobId);

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
    void AggregatePacket(Ptr<Packet> p, Ipv4Header header, uint16_t sport, Ptr<Ipv4Interface> incomingInterface);
    void ForwardUp(Ptr<Packet> p, Ipv4Header header, uint16_t sport, Ptr<Ipv4Interface> incomingInterface);

    // 处理ack包
    void ResetEcnTimer();
    void ReceiveAck(ATPTag atpTag);
    void SendAck(ATPTag atpTag, Ipv4Header ipHeader);
    void SendMultiAck(const ATPTag& atpTag, const Ipv4Header& ipHeader);

    // 重传数据包
    void Retransmit();
    
    // 检查超时的数据包
    void CheckRetransmitTimeout();

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
    
    bool m_ecnTimerRunning = false;                         // 标记ECN计时器是否在运行
    EventId m_ecnTimerEvent;                        // 添加ECN计时器
    EventId m_sendWindowDataEvent{};                //!< micro-delay event to send pending data
    EventId m_retxEvent{};                          //!< Retransmission event
    EventId m_retxTimeoutCheckEvent{};              //!< Retransmission timeout check event
    EventId m_sendAckEvent{};                       //!< Send ACK event
    EventId m_sendMultiAckEvent{};                  //!< Send ACK event

    // 接收发送缓冲区
    bool m_allowBroadcast{false};                   // 是否允许广播
    uint32_t m_rxBufferSize;                        // 接收缓冲区大小
    uint32_t m_txBufferSize;                        // 发送缓冲区大小
    uint32_t m_rxAvailable;                         // 接收缓冲区可接收数据量
    uint32_t m_txAvailable;                         // 发送缓冲区可发送数据量
    Ptr<ATPTxBuffer> m_txBuffer;                    // 发送缓冲区，自定义的类型
    std::queue<std::pair<Ptr<Packet>, Address>> m_rxBuffer; // 接收缓冲区，是个队列
    std::unordered_map<std::pair<uint8_t, uint32_t>, JobAggregator, PairHash> m_aggregators;

    // 记录发送的总字节数
    uint64_t m_totalTxBytes = 0;

    // 拥塞控制
    uint32_t m_nextSeqNo;              // 下一个序列号
    uint32_t m_highestRxSeqNo;         // 最高接收序列号
    uint32_t m_initCwnd;               // 初始拥塞窗口
    uint32_t m_ssthresh;               // 慢启动阈值
    uint32_t m_mss;                    // 最大报文段大小
    
    // 超时重传参数
    Time m_retxTimeout{MicroSeconds(500)};  // 重传超时时间，默认500微秒
    Time m_retxCheckInterval{MicroSeconds(100)};  // 超时检查间隔，默认100微秒
    
    // Ipv4EndPoint* m_endPoint;          // 本地端点
    // Address m_peerAddress;             // 对端地址
    TracedCallback<Ptr<const Packet>> m_txTrace;  // 发送跟踪
    TracedCallback<Ptr<const Packet>> m_rxTrace;  // 接收跟踪

    // 存储不同jobId : <bitmap0, bitmap1>
    std::map<uint8_t, std::pair<uint32_t, uint32_t>> m_jobBitmapMap;

    // 存储已知节点的IP地址和端口映射
    std::map<uint8_t, std::vector<std::pair<Ipv4Address, uint16_t>>> m_jobAddressMap;

    // 存储每个job的聚合树结构
    std::map<uint8_t, JobTree> m_jobTreeMap;
    
    // 存储不同jobId的bitmap映射表: key=(bitmap0, bitmap1), value=flat_bitmap
    std::map<uint8_t, std::map<std::pair<uint32_t, uint32_t>, uint32_t>> m_jobBitmapMappingTable;
    
    // 存储每个job期望的完整 bitmap1（用于判断聚合是否完成）
    std::map<uint8_t, uint32_t> m_jobExpectedFlatBitmapMap;
};

} // namespace ns3

#endif /* ATP_SOCKET_H */
