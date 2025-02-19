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
    m_initCwnd = 2;
    // 重传
    m_rto = MilliSeconds(200);

    m_allowBroadcast = false;
    m_txBufferSize = 596; // 默认32KB，568 = 2 packet, 用来测试数据。
    m_rxBufferSize = 596; // 这个暂时不知道啥用
    m_txAvailable = m_txBufferSize;
    m_rxAvailable = 0;

    m_txBuffer = CreateObject<ATPTxBuffer>();
    m_txBuffer->SetMaxBufferSize(m_txBufferSize); // 设置发送缓冲区最大容量
    m_txBuffer->SetCwnd(m_initCwnd);
    // m_rxBuffer已经定义了

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
        m_rxAvailable -= p->GetSize();
    }
    else
    {
        p = nullptr;
    }
    return p;
}

void
ATPSocket::StartRetransmitTimer()
{
    NS_LOG_FUNCTION(this);
    if (m_retxEvent.IsExpired())
    {
        m_retxEvent = Simulator::Schedule(m_rto, &ATPSocket::RetransmitExpired, this);
        NS_LOG_INFO("Start retransmit timer at " << Simulator::Now().GetSeconds() 
                    << "s, will expire at " 
                    << (Simulator::Now() + m_rto).GetSeconds() << "s");
    }
}

void
ATPSocket::RetransmitExpired()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO("Retransmit timer expired at " << Simulator::Now().GetSeconds() << "s");

    m_initCwnd = m_initCwnd / 2;
    NS_LOG_INFO("Retransmit timer expired, update congestion window to " << m_initCwnd);

    // 重新发送数据
    SendWindowData();
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
    uint32_t availableWindow = m_txBuffer->GetCwnd();
    
    // 发送窗口内的数据
    for (uint32_t i = 0; i < availableWindow; i++)
    {
        Ptr<Packet> packet = m_txBuffer->SendPacket();
        if (packet == nullptr) {
            break;
        }
        // 发送数据包
        DoSend(packet);
    }
    

    /*
    // 检查已发送缓冲区(m_sentQueue)是否为空，也就是窗口
    // 如果不为空，说明有数据包的ACK未收到，需要重传
    // 只有当已发送缓冲区为空时，才发送新数据
    if (m_txBuffer->IsSentBufferEmpty())
    {
        while (sentPacketNum < availableWindow)
        {

            // 还得考虑到发送缓冲区的数据短于拥塞窗口的情况
            // 那就得通知上层塞数据了
            Ptr<Packet> packet = m_txBuffer->NextPacket();
            if (packet == nullptr)
            {
                NS_LOG_INFO("No packet to send");
                break;
            }
            else
            {
                // 启动重传定时器
                StartRetransmitTimer();
                DoSend(packet);
                sentPacketNum++;
            }
        }

        return sentPacketNum;
    }
    else
    {
        // 重传
        NS_LOG_INFO("sent buffer is not empty, there are packets waiting to retransmit");

        
        // 发送m_txBuffer中sent的包
        while (!m_txBuffer->IsSentBufferEmpty())
        {
            Ptr<Packet> packet = m_txBuffer->NextRetransmitPacket();
            DoSend(packet);
            sentPacketNum++;
        }

        return sentPacketNum;
    }
    */
}

int
ATPSocket::DoSend(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);

    if (Ipv4Address::IsMatchingType(m_defaultAddress))
    {
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
ATPSocket::ReceiveAck(ATPHeader atpHeader)
{
    NS_LOG_FUNCTION(this << atpHeader);
    // 打印接收ack的时间
    NS_LOG_INFO("Receive ack at time" << Simulator::Now().GetSeconds() << "s");
    NS_LOG_INFO("Process ack number " << atpHeader.GetAckNumber());

    // 1. 更新已发送缓冲区
    m_txBuffer->ProcessAck(atpHeader.GetAckNumber());
    
    // 2.取消当前的重传定时器
    if (!m_retxEvent.IsExpired())
    {
        // 调整rto，rto是什么？
        m_retxEvent.Cancel();
        m_rto = Max(MilliSeconds(200), m_rto - MilliSeconds(10));
    }
    /*else
    {
        // 如果是重传后收到的ACK，增加RTO
        m_rto = Min(Seconds(1), m_rto * 2);
        NS_LOG_INFO("Adjusted RTO to " << m_rto.GetMilliSeconds() << "ms");
    }*/

    // 通知上层继续发送数据到缓冲区
    NS_LOG_INFO("Notify ATPBulkSendApplication to send data");
    NotifySend(0);
}

void
ATPSocket::SendAck(ATPHeader atpHeader, Ipv4Header ipHeader)
{
    NS_LOG_FUNCTION(this << atpHeader << ipHeader);

    // 返回一个ack数据包给发送端
    ATPHeader ackHeader;
    ackHeader.SetFlags(ATPHeader::ACK);
    ackHeader.SetJobId(atpHeader.GetJobId());
    ackHeader.SetAckNumber(atpHeader.GetSeqNumber());

    // ack包的源地址和目的地址与发送的包相反
    Ipv4Address ackSource = ipHeader.GetDestination();
    Ipv4Address ackDestination = ipHeader.GetSource();
    uint16_t ackSourcePort = atpHeader.GetDestinationPort();
    uint16_t ackDestinationPort = atpHeader.GetSourcePort();

    ackHeader.SetSourcePort(ackSourcePort);
    ackHeader.SetDestinationPort(ackDestinationPort);

    Ptr<Packet> ackPacket = Create<Packet>();
    ackPacket->AddHeader(ackHeader);

    NS_LOG_INFO("Send ack packet from " << ackSource << ":" << ackSourcePort
                << " to " << ackDestination << ":" << ackDestinationPort 
                << " at time " << Simulator::Now().GetSeconds() << "s");
    // 发送ack包, 源地址和目的地址与发送的包相反
    m_atp->Send(ackPacket, ackSource, ackDestination,
                ackSourcePort, ackDestinationPort);
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
        return;
    }

    // 判断是否是ack数据包，可能ackNumber不对，应该选别的。
    ATPHeader atpHeader;
    packet->PeekHeader(atpHeader);
    if (atpHeader.GetFlags() & ATPHeader::ACK)
    {
        // 处理ack包
        ReceiveAck(atpHeader);
    }
    else
    {
        // 如果接收缓冲区有足够的空间，将数据包放入接收队列
        if ((m_rxAvailable + packet->GetSize()) <= m_rxBufferSize)
        {
            Address address = InetSocketAddress(header.GetSource(), port);

            packet->RemoveHeader(atpHeader);
            m_rxBuffer.emplace(packet, address);
            m_rxAvailable += packet->GetSize();
            // 通知应用层有数据可读
            NS_LOG_INFO("NotifyDataRecv");
            NotifyDataRecv();

            // 发送ack包
            SendAck(atpHeader, header);
        }
        else
        {
            NS_LOG_WARN("No receive buffer space available.  Drop.");
            m_dropTrace(packet);
        }
    }
}

}
