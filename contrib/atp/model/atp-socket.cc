#include "atp-socket.h"
#include "atp-l4-protocol.h"

#include "ns3/ipv4-end-point.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/packet.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ATPSocket");

NS_OBJECT_ENSURE_REGISTERED(ATPSocket);

TypeId
ATPSocket::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ATPSocket")
        .SetParent<Socket>()
        .SetGroupName("Internet")
        .AddTraceSource("Drop",
                       "Drop packet due to receive buffer overflow",
                       MakeTraceSourceAccessor(&ATPSocket::m_dropTrace),
                       "ns3::Packet::TracedCallback")
        .AddTraceSource("Tx",
                       "Send packet",
                       MakeTraceSourceAccessor(&ATPSocket::m_txTrace),
                       "ns3::Packet::TracedCallback")
        .AddTraceSource("Rx",
                       "Receive packet",
                       MakeTraceSourceAccessor(&ATPSocket::m_rxTrace),
                       "ns3::Packet::TracedCallback");
    return tid;
}

// 考虑将这么些变量放入函数内部赋值
ATPSocket::ATPSocket()
    : Socket()
{
    NS_LOG_FUNCTION(this);
    m_endPoint = nullptr;
    m_node = nullptr;
    m_atp = nullptr;

    m_errno = ERROR_NOTERROR;
    m_shutdownSend = false;
    m_shutdownRecv = false;
    m_connected = false;
    
    // 初始拥塞控制窗口
    m_initCwnd = 1;

    m_allowBroadcast = false;
    m_txBufferSize = 248 * 2000;
    m_rxBufferSize = 248 * 2000;
    m_txAvailable = m_txBufferSize;
    m_rxAvailable = m_rxBufferSize;

    m_txBuffer = CreateObject<ATPTxBuffer>();
    m_txBuffer->SetMaxBufferSize(m_txBufferSize); // 设置发送缓冲区最大容量
    m_txBuffer->SetCwnd(m_initCwnd);
    // m_rxBuffer已经定义了

    m_aggregators.resize(MAX_AGGREGATORS);
}

ATPSocket::~ATPSocket()
{
    NS_LOG_FUNCTION(this);
    m_node = nullptr;
    if (m_endPoint != nullptr)
    {
        m_endPoint->SetDestroyCallback(MakeNullCallback<void>());
        m_atp->DeAllocate(m_endPoint);
        m_endPoint = nullptr;
    }
    m_atp = nullptr;
}

void
ATPSocket::SetInitCwnd(uint32_t initCwnd)
{
    NS_LOG_FUNCTION(this << initCwnd);
    m_initCwnd = initCwnd;
    m_txBuffer->SetCwnd(m_initCwnd);
}

void
ATPSocket::SetRetxTimeout(Time timeout)
{
    NS_LOG_FUNCTION(this << timeout);
    m_retxTimeout = timeout;
}

void
ATPSocket::SetRetxCheckInterval(Time interval)
{
    NS_LOG_FUNCTION(this << interval);
    m_retxCheckInterval = interval;
}

void
ATPSocket::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

Ptr<Node>
ATPSocket::GetNode() const
{
    NS_LOG_FUNCTION(this);
    return m_node;
}

void 
ATPSocket::SetATP(Ptr<ATPL4Protocol> atp)
{
    NS_LOG_FUNCTION(this << atp);
    m_atp = atp;
}

Ptr<ATPL4Protocol>
ATPSocket::GetATP() const
{
    NS_LOG_FUNCTION(this);
    return m_atp;
}

void
ATPSocket::SetRxBufferSize(uint32_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_rxBufferSize = size;
}

uint32_t
ATPSocket::GetRxBufferSize() const
{
    NS_LOG_FUNCTION(this);
    return m_rxBufferSize;
}

uint32_t
ATPSocket::GetRxAvailable() const
{
    NS_LOG_FUNCTION(this);
    return m_rxAvailable;
}

uint32_t
ATPSocket::GetTxAvailable() const
{
    NS_LOG_FUNCTION(this);
    return m_txAvailable;
}

Socket::SocketErrno
ATPSocket::GetErrno() const
{
    NS_LOG_FUNCTION(this);
    return m_errno;
}

Socket::SocketType
ATPSocket::GetSocketType() const
{
    NS_LOG_FUNCTION(this);
    return NS3_SOCK_SEQPACKET;
}

int
ATPSocket::GetSockName(Address& address) const
{
    NS_LOG_FUNCTION(this << address);
    if (m_endPoint != nullptr)
    {
        address = InetSocketAddress(m_endPoint->GetLocalAddress(), m_endPoint->GetLocalPort());
    }
    else
    {
        address = InetSocketAddress(Ipv4Address::GetZero(), 0);
    }
    return 0;
}

int
ATPSocket::GetPeerName(Address& address) const
{
    NS_LOG_FUNCTION(this << address);
    if (!m_connected)
    {
        m_errno = ERROR_NOTCONN;
        return -1;
    }
    if (Ipv4Address::IsMatchingType(m_defaultAddress))
    {
        address = InetSocketAddress(Ipv4Address::ConvertFrom(m_defaultAddress), m_defaultPort);
    }
    else
    {
        NS_ASSERT_MSG(false, "unexpected address type");
    }
    return 0;
}

bool
ATPSocket::SetAllowBroadcast(bool allowBroadcast)
{
    NS_LOG_FUNCTION(this << allowBroadcast);
    m_allowBroadcast = allowBroadcast;
    return true;
}

bool
ATPSocket::GetAllowBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return m_allowBroadcast;
}

void
ATPSocket::CancelAllTimers()
{
    NS_LOG_FUNCTION(this);
    m_sendWindowDataEvent.Cancel();
    m_retxEvent.Cancel();
    m_retxTimeoutCheckEvent.Cancel();
    m_ecnTimerEvent.Cancel();
    m_sendAckEvent.Cancel();
    m_sendMultiAckEvent.Cancel();
}

void
ATPSocket::Destroy()
{
    NS_LOG_FUNCTION(this);
    m_endPoint = nullptr;
    if (m_atp != nullptr)
    {
        CancelAllTimers();
        m_atp->RemoveSocket(this);
    }
    CancelAllTimers();
}

void
ATPSocket::DeallocateEndPoint()
{
    NS_LOG_FUNCTION(this);
    if (m_endPoint != nullptr)
    {
        CancelAllTimers();
        m_endPoint->SetDestroyCallback(MakeNullCallback<void>());
        m_atp->DeAllocate(m_endPoint);
        m_endPoint = nullptr;
        m_atp->RemoveSocket(this);
    }
}

int
ATPSocket::FinishBind()
{
    NS_LOG_FUNCTION(this);
    bool done = false;
    if (m_endPoint != nullptr)
    {
        m_endPoint->SetRxCallback(
            MakeCallback(&ATPSocket::ForwardUp, Ptr<ATPSocket>(this)));
        m_endPoint->SetDestroyCallback(
            MakeCallback(&ATPSocket::Destroy, Ptr<ATPSocket>(this)));
        done = true;
    }

    if (done)
    {
        m_shutdownRecv = false;
        m_shutdownSend = false;
        return 0;
    }
    return -1;
}

int
ATPSocket::Bind()
{
    NS_LOG_FUNCTION(this);
    m_endPoint = m_atp->Allocate();
    if (m_boundnetdevice)
    {
        m_endPoint->BindToNetDevice(m_boundnetdevice);
    }
    return FinishBind();
}

int
ATPSocket::Bind(const Address& address)
{
    NS_LOG_FUNCTION(this << address);

    if (InetSocketAddress::IsMatchingType(address))
    {
        NS_ASSERT_MSG(m_endPoint == nullptr, "Endpoint already allocated.");
        NS_ASSERT_MSG(m_endPoint == nullptr, "Endpoint already allocated.");

        InetSocketAddress transport = InetSocketAddress::ConvertFrom(address);
        Ipv4Address ipv4 = transport.GetIpv4();
        uint16_t port = transport.GetPort();
        if (ipv4 == Ipv4Address::GetAny() && port == 0)
        {
            m_endPoint = m_atp->Allocate();
        }
        else if (ipv4 == Ipv4Address::GetAny() && port != 0)
        {
            m_endPoint = m_atp->Allocate(GetBoundNetDevice(), port);
        }
        else if (ipv4 != Ipv4Address::GetAny() && port == 0)
        {
            m_endPoint = m_atp->Allocate(ipv4);
        }
        else if (ipv4 != Ipv4Address::GetAny() && port != 0)
        {
            m_endPoint = m_atp->Allocate(GetBoundNetDevice(), ipv4, port);
        }
        if (nullptr == m_endPoint)
        {
            m_errno = port ? ERROR_ADDRINUSE : ERROR_ADDRNOTAVAIL;
            return -1;
        }
        if (m_boundnetdevice)
        {
            m_endPoint->BindToNetDevice(m_boundnetdevice);
        }
    }
    else
    {
        NS_LOG_ERROR("Not IsMatchingType");
        m_errno = ERROR_INVAL;
        return -1;
    }

    return FinishBind();
}

int
ATPSocket::Bind6()
{
    NS_LOG_FUNCTION(this);
    return -1;
}

void
ATPSocket::BindToNetDevice(Ptr<NetDevice> netdevice)
{
    NS_LOG_FUNCTION(this << netdevice);
    Socket::BindToNetDevice(netdevice);
    if (m_endPoint)
    {
        m_endPoint->BindToNetDevice(netdevice);
    }
}

int
ATPSocket::ShutdownSend()
{
    NS_LOG_FUNCTION(this);
    m_shutdownSend = true;
    return 0;
}

int
ATPSocket::ShutdownRecv()
{
    NS_LOG_FUNCTION(this);
    m_shutdownRecv = true;
    return 0;
}

int
ATPSocket::Close()
{
    NS_LOG_FUNCTION(this);
    if (m_shutdownRecv && m_shutdownSend)
    {
        m_errno = Socket::ERROR_BADF;
        return -1;
    }
    m_shutdownRecv = true;
    m_shutdownSend = true;
    DeallocateEndPoint();
    return 0;
}

int
ATPSocket::Connect(const Address& address)
{
    NS_LOG_FUNCTION(this << address);
    if (InetSocketAddress::IsMatchingType(address))
    {
        InetSocketAddress transport = InetSocketAddress::ConvertFrom(address);
        m_defaultAddress = Address(transport.GetIpv4());
        m_defaultPort = transport.GetPort();
        m_connected = true;
        
        // 启动超时检查定时器
        if (!m_retxTimeoutCheckEvent.IsPending()) {
            m_retxTimeoutCheckEvent = Simulator::Schedule(m_retxCheckInterval,
                                                           &ATPSocket::CheckRetransmitTimeout,
                                                           this);
        }
        
        NotifyConnectionSucceeded(); // 通知连接成功，由应用层来设置回调
    }
    else
    {
        NotifyConnectionFailed(); // 通知连接失败，由应用层来设置回调
        return -1;
    }

    return 0;
}

int
ATPSocket::Listen()
{
    NS_LOG_FUNCTION(this);
    m_errno = Socket::ERROR_OPNOTSUPP;
    return -1;
}

Ptr<Packet>
ATPSocket::Recv(uint32_t maxSize, uint32_t flags)
{
    NS_LOG_FUNCTION(this << maxSize << flags);

    Address fromAddress;
    Ptr<Packet> packet = RecvFrom(maxSize, flags, fromAddress);
    return packet;
}

Ptr<Packet>
ATPSocket::RecvFrom(uint32_t maxSize, uint32_t flags, Address& fromAddress)
{
    //NS_LOG_FUNCTION(this << maxSize << flags << fromAddress);
    NS_LOG_INFO("ATPRecvFromTEST");
    if (m_rxBuffer.empty())
    {
        m_errno = ERROR_AGAIN;
        return nullptr;
    }
    Ptr<Packet> p = m_rxBuffer.front().first;
    fromAddress = m_rxBuffer.front().second;

    if (p->GetSize() <= maxSize)
    {
        m_rxBuffer.pop();
        m_rxAvailable += p->GetSize();
    }
    else
    {
        p = nullptr;
    }
    return p;
}

int
ATPSocket::Send(Ptr<Packet> p, uint32_t flags)
{
    NS_LOG_FUNCTION(this << p);

    if (m_connected)
    {
        // 绑定和连接
        if (m_boundnetdevice)
        {
            NS_LOG_LOGIC("Bound interface number " << m_boundnetdevice->GetIfIndex());
        }
        if (m_endPoint == nullptr && Ipv4Address::IsMatchingType(m_defaultAddress))
        {
            if (Bind() == -1)
            {
                NS_ASSERT(m_endPoint == nullptr);
                return -1;
            }
            NS_ASSERT(m_endPoint != nullptr);
        }
        if (m_shutdownSend)
        {
            m_errno = ERROR_SHUTDOWN;
            return -1;
        }

        // 将数据包填入发送缓冲区
        if (!m_txBuffer->AddPacket(p))
        {
            m_errno = ERROR_MSGSIZE;
            return -1;
        }
        
        if (!m_sendWindowDataEvent.IsPending()) {
            m_sendWindowDataEvent = Simulator::Schedule(TimeStep(1),
                                                        &ATPSocket::SendWindowData,
                                                        this);
        }

        return p->GetSize();
    }
    else 
    {
        m_errno = ERROR_NOTCONN;
        return -1;
    }
}

// 根据拥塞窗口，发送缓冲区中的数据
void
ATPSocket::SendWindowData()
{
    NS_LOG_FUNCTION(this);
    // 获取窗口信息
    uint32_t leftBound = m_txBuffer->GetCwndLeftBound();
    uint32_t cwnd = m_txBuffer->GetCwnd();
    uint32_t pendingId = m_txBuffer->GetPendingFrontPacketId();

    NS_LOG_INFO("SendWindowData - Time: " << Simulator::Now().GetSeconds() 
                << " leftBound: " << leftBound 
                << " cwnd: " << cwnd 
                << " pendingFrontId: " << pendingId);

    // 当cwnd为1时的特殊处理
    if (cwnd == 1) {
        NS_LOG_INFO("Special handling for cwnd=1");
        // 确保在窗口范围内发送，并且不会过于激进
        if (pendingId <= leftBound + cwnd) {
            Ptr<Packet> packet = m_txBuffer->SendPacket();
            if (packet == nullptr) {
                NS_LOG_INFO("No packet to send in cwnd=1 case");
                return;
            }
            NS_LOG_INFO("Sending packet in cwnd=1 case");
            DoSend(packet);
            
            // 添加随机延迟，避免一个流占用所有资源
            /*
            if (!m_sendWindowDataEvent.IsPending()) {
                Time delay = MilliSeconds(10 + (rand() % 20)); // 10-30ms的随机延迟
                m_sendWindowDataEvent = Simulator::Schedule(delay,
                                                        &ATPSocket::SendWindowData,
                                                        this);
            }
            */
        }
        return;
    }

    // 发送一个窗口内的数据
    while (m_txBuffer->GetPendingFrontPacketId() <= leftBound + cwnd) {
        Ptr<Packet> packet = m_txBuffer->SendPacket();
        if (packet == nullptr) {
            NS_LOG_INFO("No more packets to send in window");
            break;
        }
        NS_LOG_INFO("Sending packet in window");
        DoSend(packet);
    }
}

int
ATPSocket::DoSend(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);

    if (Ipv4Address::IsMatchingType(m_defaultAddress))
    {
        m_totalTxBytes += p->GetSize();
        return DoSendTo(p, Ipv4Address::ConvertFrom(m_defaultAddress), m_defaultPort);
    }

    return -1;
}

int
ATPSocket::DoSendTo(Ptr<Packet> p, Ipv4Address dest, uint16_t port)
{
    NS_LOG_FUNCTION(this << p << dest << port);

    Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();

    if (m_endPoint->GetLocalAddress() != Ipv4Address::GetAny())
    {
        NS_LOG_INFO("Send packet to " << dest << ":" << port);
        // 5. 发送数据包
        m_atp->Send(p->Copy(),
                    m_endPoint->GetLocalAddress(),
                    dest,
                    m_endPoint->GetLocalPort(),
                    port);
        return p->GetSize();
    }
    else if (ipv4->GetRoutingProtocol())
    {
        Ipv4Header header;
        header.SetDestination(dest);
        header.SetProtocol(ATPL4Protocol::PROT_NUMBER);
        Socket::SocketErrno errno_;
        Ptr<Ipv4Route> route;
        Ptr<NetDevice> oif = m_boundnetdevice; // specify non-zero if bound to a specific device
        // TBD-- we could cache the route and just check its validity
        route = ipv4->GetRoutingProtocol()->RouteOutput(p, header, oif, errno_);
        if (route)
        {
            NS_LOG_LOGIC("Route exists");

            header.SetSource(route->GetSource());
            m_atp->Send(p->Copy(),
                        header.GetSource(),
                        header.GetDestination(),
                        m_endPoint->GetLocalPort(),
                        port,
                        route);
            return p->GetSize();
        }
        else
        {
            NS_LOG_LOGIC("No route to destination");
            NS_LOG_ERROR("ERROR_NOROUTETOHOST");
            m_errno = ERROR_NOROUTETOHOST;
            return -1;
        }
    }
    return 0;
}

int
ATPSocket::SendTo(Ptr<Packet> p, uint32_t flags, const Address& address)
{
    NS_LOG_FUNCTION(this << p << flags << address);

    if (InetSocketAddress::IsMatchingType(address))
    {
        InetSocketAddress transport = InetSocketAddress::ConvertFrom(address);
        Ipv4Address ipv4 = transport.GetIpv4();
        uint16_t port = transport.GetPort();
        return DoSendTo(p, ipv4, port);
    }
    else
    {
        NS_LOG_ERROR("Not IsMatchingType");
        m_errno = ERROR_INVAL;
        return -1;
    }
}

void
ATPSocket::Retransmit()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO("Retransmit at time " << Simulator::Now().GetSeconds() << "s");

    while (m_txBuffer->HasPacketToRetransmit()) {
        Ptr<Packet> packet = m_txBuffer->RetransmitPacket();
        DoSend(packet);
    }
}

void
ATPSocket::CheckRetransmitTimeout()
{
    NS_LOG_FUNCTION(this);
    
    // 检查是否有超时的数据包（使用微秒）
    uint32_t timeoutUs = static_cast<uint32_t>(m_retxTimeout.GetMicroSeconds());
    uint32_t timeoutCount = m_txBuffer->CheckAndMoveTimeoutPackets(timeoutUs);
    
    if (timeoutCount > 0) {
        NS_LOG_WARN("CheckRetransmitTimeout: Found " << timeoutCount 
                    << " timeout packets at time " << Simulator::Now().GetSeconds() << "s");
        
        // 触发重传
        Retransmit();
        
        // 由于发生超时，可能需要减小拥塞窗口（类似于ECN机制）
        if (!m_ecnTimerRunning) {
            m_txBuffer->ProcessCongestion(true);
            m_ecnTimerEvent = Simulator::Schedule(MicroSeconds(200), &ATPSocket::ResetEcnTimer, this);
            m_ecnTimerRunning = true;
        }
    }
    
    // 继续调度下一次检查
    if (!m_shutdownSend && m_connected) {
        m_retxTimeoutCheckEvent = Simulator::Schedule(m_retxCheckInterval,
                                                       &ATPSocket::CheckRetransmitTimeout,
                                                       this);
    }
}

void
ATPSocket::ResetEcnTimer()
{
    NS_LOG_FUNCTION(this);
    m_ecnTimerRunning = false;
}

void
ATPSocket::ReceiveAck(ATPTag atpTag)
{
    NS_LOG_FUNCTION(this << atpTag);
    // 打印接收ack的时间
    NS_LOG_INFO("Receive ack packet at time " << Simulator::Now().GetSeconds() << "s");

    // 判断收到的ack是否按序到达
    if (m_txBuffer->IsOrderedAck(atpTag.GetSeqNum()))
    {
        // 处理按序到达的ack
        m_txBuffer->ProcessOrderedAck(atpTag.GetSeqNum());
        m_txBuffer->UpdateCwndLeftBound(atpTag.GetSeqNum());

        bool isEcn = (atpTag.GetEcn() == 1);

        // ECN标记且计时器未运行时，减半窗口并启动计时器
        if (isEcn && !m_ecnTimerRunning) {
            m_txBuffer->ProcessCongestion(true);
            m_ecnTimerEvent = Simulator::Schedule(MicroSeconds(200), &ATPSocket::ResetEcnTimer, this);
            m_ecnTimerRunning = true;
        } 
        if (!isEcn) {
            m_txBuffer->ProcessCongestion(false);
        }

        if (!m_sendWindowDataEvent.IsPending()) {
            m_sendWindowDataEvent = Simulator::Schedule(TimeStep(1),
                                                        &ATPSocket::SendWindowData,
                                                        this);
        }
    }
    else
    {
        // 处理乱序到达的ack
        Ptr<Packet> packet = m_txBuffer->ProcessUnorderedAck(atpTag.GetSeqNum());
        if (packet != nullptr) {
            if (!m_ecnTimerRunning) {
                m_txBuffer->ProcessCongestion(true);
                m_ecnTimerEvent = Simulator::Schedule(MicroSeconds(200), &ATPSocket::ResetEcnTimer, this);
                m_ecnTimerRunning = true;
            }
            // 执行重传函数
            Retransmit();
        }
    }

    // 通知上层继续发送数据到缓冲区
    NS_LOG_INFO("Notify ATPBulkSendApplication to send data");
    NotifySend(0);
}

void
ATPSocket::SendAck(ATPTag atpTag, Ipv4Header ipHeader)
{
    NS_LOG_FUNCTION(this << atpTag << ipHeader);

    // 返回一个ack数据包给发送端
    ATPTag ackTag;
    ackTag.m_isAck = 1;
    ackTag.SetEcn(atpTag.GetEcn());
    ackTag.SetJobId(atpTag.GetJobId());
    ackTag.SetSeqNum(atpTag.GetSeqNum());  // 添加这行：设置seqNumber与原始数据包一致

    // ack包的源地址和目的地址与发送的包相反
    Ipv4Address ackSource = ipHeader.GetDestination();
    Ipv4Address ackDestination = ipHeader.GetSource();
    uint16_t ackSourcePort = atpTag.GetDestinationPort();
    uint16_t ackDestinationPort = atpTag.GetSourcePort();

    ackTag.SetSourcePort(ackSourcePort);
    ackTag.SetDestinationPort(ackDestinationPort);

    Ptr<Packet> ackPacket = Create<Packet>(10); 
    ackPacket->AddPacketTag(ackTag);

    NS_LOG_INFO("Send ack packet from " << ackSource << ":" << ackSourcePort
                << " to " << ackDestination << ":" << ackDestinationPort 
                << " at time " << Simulator::Now().GetSeconds() << "s");
    // 发送ack包, 源地址和目的地址与发送的包相反
    m_atp->Send(ackPacket, ackSource, ackDestination,
                ackSourcePort, ackDestinationPort);
}


void
ATPSocket::SendMultiAck(const ATPTag& atpTag, const Ipv4Header& ipHeader)
{
    NS_LOG_FUNCTION(this << atpTag << ipHeader);
    NS_LOG_INFO("Send multi-ack");
    
    // 获取该作业的地址映射
    const auto& addrList = GetAddressMapping(atpTag.GetJobId());
    
    // 检查是否有地址映射
    if (addrList.empty()) {
        NS_LOG_WARN("No address mapping found for job ID " << 
                    static_cast<uint32_t>(atpTag.GetJobId()));
        return;
    }
    
    // 向所有映射的地址发送ACK
    for (const auto& addrPair : addrList)
    {
        // 创建ACK头部
        ATPTag ackTag;
        ackTag.m_isAck = 1;
        ackTag.SetEcn(atpTag.GetEcn());
        ackTag.SetJobId(atpTag.GetJobId());
        ackTag.SetSeqNum(atpTag.GetSeqNum());  // 添加这行：设置seqNumber与原始数据包一致
        
        // ack包的源地址和目的地址与发送的包相反
        Ipv4Address ackSource = ipHeader.GetDestination();
        uint16_t ackSourcePort = atpTag.GetDestinationPort();

        Ipv4Address ackDestination = addrPair.first;
        uint16_t ackDestinationPort = addrPair.second;

        // 设置端口
        ackTag.SetSourcePort(ackSourcePort);
        ackTag.SetDestinationPort(ackDestinationPort);
        
        Ptr<Packet> ackPacket = Create<Packet>(10);
        ackPacket->AddPacketTag(ackTag);
        
        NS_LOG_INFO("Send multi-ack from " << ipHeader.GetDestination() 
                    << ":" << atpTag.GetDestinationPort()
                    << " to " << addrPair.first << ":" << addrPair.second
                    << " at time " << Simulator::Now().GetSeconds() << "s");
        
        // 向映射地址发送ACK
        m_atp->Send(ackPacket, ackSource, ackDestination,
                    ackSourcePort, ackDestinationPort);
    }
}

uint32_t
ATPSocket::ConvertToFlatBitmap(uint32_t bitmap0, uint32_t bitmap1, uint8_t fanInDegree0)
{
    uint32_t flat_bitmap = 0;
    
    // 遍历 bitmap1 中所有设置的位（第二层聚合点）
    for (uint32_t b1_pos = 0; b1_pos < 32; b1_pos++) {
        if (bitmap1 & (1u << b1_pos)) {
            // 遍历 bitmap0 中所有设置的位（第一层聚合点）
            for (uint32_t b0_pos = 0; b0_pos < 32; b0_pos++) {
                if (bitmap0 & (1u << b0_pos)) {
                    // 计算在 flat_bitmap 中的位置
                    uint32_t flat_pos = b1_pos * fanInDegree0 + b0_pos;
                    flat_bitmap |= (1u << flat_pos);
                }
            }
        }
    }
    
    NS_LOG_INFO("ConvertToFlatBitmap: bitmap0=0x" << std::hex << bitmap0 
                << " bitmap1=0x" << bitmap1 
                << " fanInDegree0=" << std::dec << (uint32_t)fanInDegree0
                << " -> flat_bitmap=0x" << std::hex << flat_bitmap);
    
    return flat_bitmap;
}

void
ATPSocket::AggregatePacket(Ptr<Packet> packet, 
                           Ipv4Header header,
                           uint16_t port,
                           Ptr<Ipv4Interface> incomingInterface)
{
    NS_LOG_FUNCTION(this << packet << header << port << incomingInterface);

    Address address = InetSocketAddress(header.GetSource(), port);

    // 获取ATPTag
    ATPTag atpTag;
    packet->PeekPacketTag(atpTag);

    // 转换为 flat_bitmap
    uint32_t incoming_flat_bitmap = ConvertToFlatBitmap(
        atpTag.GetBitMap0(),
        atpTag.GetBitMap1(),
        atpTag.GetFaninDegree0()
    );

    // 使用哈希函数计算索引
    std::size_t index = JobAggregator::HashToIndex(
        atpTag.GetJobId(), 
        atpTag.GetSeqNum(), 
        MAX_AGGREGATORS
    );
    
    // 获取对应的聚合器
    JobAggregator& aggregator = m_aggregators[index];

    // 如果聚合器为空，或者jobId和seqNum都匹配
    if (aggregator.IsEmpty() || 
        (aggregator.m_jobId == atpTag.GetJobId() && 
         aggregator.m_seqNum == atpTag.GetSeqNum()))
    {
        // 如果是空的，初始化jobId和seqNum
        if (aggregator.IsEmpty()) {
            aggregator.m_jobId = atpTag.GetJobId();
            aggregator.m_seqNum = atpTag.GetSeqNum();
            NS_LOG_INFO("Using empty aggregator at index " << index 
                        << " for jobId " << static_cast<int>(atpTag.GetJobId())
                        << " seqNum " << atpTag.GetSeqNum());
        }

        // 处理数据包，更新 flat_bitmap
        aggregator.ProcessPacket(packet, incoming_flat_bitmap);

        // 获取期望的 flat_bitmap（所有 worker 都到达）
        uint32_t expected_flat_bitmap = 0;
        auto it = m_jobBitmapMap.find(atpTag.GetJobId());
        if (it != m_jobBitmapMap.end()) {
            // 从存储的完整 bitmap0 和 bitmap1 转换
            expected_flat_bitmap = ConvertToFlatBitmap(
                it->second.first,   // bitmap0
                it->second.second,  // bitmap1
                atpTag.GetFaninDegree0()
            );
        }

        // 检查聚合是否完成
        bool aggregationComplete = aggregator.IsComplete(expected_flat_bitmap);

        if (aggregationComplete)
        {
            NS_LOG_INFO("ATPSocket: Aggregation complete for jobId " 
                        << static_cast<int>(atpTag.GetJobId())
                        << " seqNum " << atpTag.GetSeqNum()
                        << " flat_bitmap=0x" << std::hex << aggregator.m_flat_bitmap);
            
            // 获取聚合后的数据包（用于交付）
            Ptr<Packet> aggregatedPacket = aggregator.GetResultPacket();

            // 重置聚合器，为下一个序列号做准备
            aggregator.Reset();

            // 将数据包加入接收缓冲区
            if ((m_rxAvailable - aggregatedPacket->GetSize()) >= 0)
            {
                m_rxBuffer.emplace(aggregatedPacket, address);
                m_rxAvailable -= aggregatedPacket->GetSize();

                // 调度发送 multi-ack
                if (!m_sendMultiAckEvent.IsPending()) {
                    m_sendMultiAckEvent = Simulator::Schedule(
                        TimeStep(1),
                        &ATPSocket::SendMultiAck,
                        this,
                        atpTag,
                        header
                    );
                }
                
                // 通知应用层有数据可读
                NS_LOG_INFO("NotifyDataRecv");
                NotifyDataRecv();
            }
            else
            {
                NS_LOG_WARN("ATPSocket: No receive buffer space available. Drop.");
                m_dropTrace(aggregatedPacket);
            }
        }
        else
        {
            NS_LOG_INFO("ATPSocket: Waiting for more packets. Current flat_bitmap=0x" 
                        << std::hex << aggregator.m_flat_bitmap
                        << " expected=0x" << expected_flat_bitmap);
        }
    }
    else
    {
        // 哈希冲突：这个槽位被不同的 (jobId, seqNum) 占用了
        NS_LOG_WARN("ATPSocket: Hash collision at index " << index 
                    << " expected (jobId=" << (uint32_t)aggregator.m_jobId
                    << ", seqNum=" << aggregator.m_seqNum << ")"
                    << " got (jobId=" << (uint32_t)atpTag.GetJobId()
                    << ", seqNum=" << atpTag.GetSeqNum() << ")");
        
        // 强制重置并使用新的包
        NS_LOG_WARN("Forcing aggregator reset due to collision");
        aggregator.Reset();
        aggregator.m_jobId = atpTag.GetJobId();
        aggregator.m_seqNum = atpTag.GetSeqNum();
        aggregator.ProcessPacket(packet, incoming_flat_bitmap);
    }
}

void
ATPSocket::ForwardUp(Ptr<Packet> packet, 
                     Ipv4Header header, 
                     uint16_t port,
                     Ptr<Ipv4Interface> incomingInterface)
{
    NS_LOG_FUNCTION(this << packet << header << port << incomingInterface);

    // 如果接收已关闭，丢弃数据包
    if (m_shutdownRecv)
    {
        NS_LOG_INFO("Receive is shutdown, drop the packet");
        m_dropTrace(packet);
        return;
    }

    // 判断是否是ack数据包
    ATPTag atpTag;
    bool hasATPTag = packet->PeekPacketTag(atpTag);

    if (!hasATPTag)
    {
        NS_LOG_INFO("ATP socket receive a normal packet, not ATP packet");
        m_dropTrace(packet);
        return;
    }
    
    if (atpTag.m_isAck == 1)
    {
        // 处理ack包
        ReceiveAck(atpTag);
        return;
    }

    Address address = InetSocketAddress(header.GetSource(), port);
    
    // 转换为 flat_bitmap 检查是否完全聚合
    uint32_t incoming_flat_bitmap = ConvertToFlatBitmap(
        atpTag.GetBitMap0(),
        atpTag.GetBitMap1(),
        atpTag.GetFaninDegree0()
    );
    
    // 获取期望的完整 flat_bitmap
    uint32_t expected_flat_bitmap = 0;
    auto it = m_jobBitmapMap.find(atpTag.GetJobId());
    if (it != m_jobBitmapMap.end()) {
        expected_flat_bitmap = ConvertToFlatBitmap(
            it->second.first,
            it->second.second,
            atpTag.GetFaninDegree0()
        );
    }
    
    // 检查是否是完全聚合的包
    if (incoming_flat_bitmap == expected_flat_bitmap)
    {
        NS_LOG_INFO("Received fully aggregated packet, flat_bitmap=0x" 
                    << std::hex << incoming_flat_bitmap);

        if ((m_rxAvailable - packet->GetSize()) >= 0)
        {
            m_rxBuffer.emplace(packet, address);
            m_rxAvailable -= packet->GetSize();

            if (!m_sendMultiAckEvent.IsPending()) {
                m_sendMultiAckEvent = Simulator::Schedule(
                    TimeStep(1),
                    &ATPSocket::SendMultiAck,
                    this,
                    atpTag,
                    header
                );
            }
            NS_LOG_INFO("NotifyDataRecv");
            NotifyDataRecv();
        }
        else
        {
            NS_LOG_WARN("No receive buffer space available. Drop.");
            m_dropTrace(packet);
        }
    }
    else
    {
        // 部分聚合或单个包，需要继续聚合
        NS_LOG_INFO("Received partial packet (flat_bitmap=0x" << std::hex << incoming_flat_bitmap
                    << "), needs aggregation (expected=0x" << expected_flat_bitmap << ")");
        AggregatePacket(packet, header, port, incomingInterface);
    }
}

const std::vector<std::pair<Ipv4Address, uint16_t>>& 
ATPSocket::GetAddressMapping(uint8_t jobId) const
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(jobId));
    static const std::vector<std::pair<Ipv4Address, uint16_t>> emptyList;
    
    auto it = m_jobAddressMap.find(jobId);
    if (it != m_jobAddressMap.end()) {
        return it->second;
    }
    return emptyList;
}

void
ATPSocket::AddAddressMapping(uint8_t jobId, const Ipv4Address& addr, uint16_t port)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(jobId) << addr << port);
    
    // 检查是否已存在
    auto it = m_jobAddressMap.find(jobId);
    if (it != m_jobAddressMap.end()) {
        for (const auto& pair : it->second) {
            if (pair.first == addr && pair.second == port) {
                return; // 已存在，不添加
            }
        }
        it->second.push_back(std::make_pair(addr, port));
    } else {
        std::vector<std::pair<Ipv4Address, uint16_t>> list;
        list.push_back(std::make_pair(addr, port));
        m_jobAddressMap[jobId] = list;
    }
}

void
ATPSocket::SetJobBitmap(uint8_t jobId, uint32_t bitmap0, uint32_t bitmap1)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(jobId) << bitmap0 << bitmap1);
    m_jobBitmapMap[jobId] = std::make_pair(bitmap0, bitmap1);
    NS_LOG_INFO("Set job " << static_cast<uint32_t>(jobId) 
                << " bitmap0=0x" << std::hex << bitmap0 
                << " bitmap1=0x" << bitmap1);
}

}
