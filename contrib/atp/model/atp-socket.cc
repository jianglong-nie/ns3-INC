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

// The correct maximum UDP message size is 65507, as determined by the following formula:
// 0xffff - (sizeof(IP Header) + sizeof(UDP Header)) = 65535-(20+8) = 65507

// 测一下ATP Header的大小，再重新计算头部大小
// \todo MAX_IPV4_ATP_DATAGRAM_SIZE is correct only for IPv4
static const uint32_t MAX_IPV4_ATP_DATAGRAM_SIZE = 65507; //!< Maximum UDP datagram size

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

ATPSocket::ATPSocket()
    : m_endPoint(nullptr),
      m_node(nullptr),
      m_atp(nullptr),
      m_errno(ERROR_NOTERROR),
      m_shutdownSend(false),
      m_shutdownRecv(false),
      m_connected(false),
      m_rcvBufSize(32768),
      m_rxAvailable(0),
      m_txBuffer(CreateObject<ATPTxBuffer>()),
      m_congestionControl(nullptr),
      m_nextSeqNo(0),
      m_highestRxSeqNo(0),
      m_cwnd(1),
      m_ssthresh(65535),
      m_mss(536),
      m_maxRetries(5)
{
    NS_LOG_FUNCTION(this);
}

// 需要修改，还没有写DeAllocate
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
    m_rcvBufSize = size;
}

uint32_t
ATPSocket::GetRxBufferSize() const
{
    NS_LOG_FUNCTION(this);
    return m_rcvBufSize;
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
    return MAX_IPV4_ATP_DATAGRAM_SIZE;
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
ATPSocket::Destroy()
{
    NS_LOG_FUNCTION(this);
    if (m_atp)
    {
        m_atp->RemoveSocket(this);
    }
    m_endPoint = nullptr;
}

void
ATPSocket::DeallocateEndPoint()
{
    NS_LOG_FUNCTION(this);
    if (m_endPoint)
    {
        m_atp->DeAllocate(m_endPoint);
        m_endPoint = nullptr;
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

int
ATPSocket::Send(Ptr<Packet> p, uint32_t flags)
{
    NS_LOG_FUNCTION(this << p << flags);

    if (!m_connected)
    {
        m_errno = ERROR_NOTCONN;
        return -1;
    }

    return DoSend(p);
}

int
ATPSocket::DoSend(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);
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

    if (Ipv4Address::IsMatchingType(m_defaultAddress))
    {
        return DoSendTo(p, Ipv4Address::ConvertFrom(m_defaultAddress), m_defaultPort);
    }

    m_errno = ERROR_AFNOSUPPORT;
    return -1;
}

int
ATPSocket::DoSendTo(Ptr<Packet> p, Ipv4Address dest, uint16_t port)
{
    NS_LOG_FUNCTION(this << p << dest << port);

    // 1. 检查是否绑定了端口
    if (m_endPoint == nullptr)
    {
        if (Bind() == -1)
        {
            return -1;
        }
    }

    // 2. 检查是否关闭发送
    if (m_shutdownSend)
    {
        m_errno = ERROR_SHUTDOWN;
        return -1;
    }

    Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();

    if (m_endPoint->GetLocalAddress() != Ipv4Address::GetAny())
    {

        if (!m_txBuffer->Add(p))
        {
            m_errno = ERROR_MSGSIZE;
            return -1;
        }

        Ptr<Packet> packet = m_txBuffer->NextPacket();
        NS_LOG_INFO("Send packet to " << dest << ":" << port);
        // 5. 发送数据包
        m_atp->Send(packet->Copy(),
                    m_endPoint->GetLocalAddress(),
                    dest,
                    m_endPoint->GetLocalPort(),
                    port);

        // 6. 通知发送完成
        NotifyDataSent(p->GetSize());
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
            NotifyDataSent(p->GetSize());
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
    NS_LOG_FUNCTION(this << maxSize << flags << fromAddress);
    
    if (m_deliveryQueue.empty())
    {
        m_errno = ERROR_AGAIN;
        return nullptr;
    }
    Ptr<Packet> p = m_deliveryQueue.front().first;
    fromAddress = m_deliveryQueue.front().second;

    if (p->GetSize() <= maxSize)
    {
        m_deliveryQueue.pop();
        m_rxAvailable -= p->GetSize();
    }
    else
    {
        p = nullptr;
    }
    return p;
}

void
ATPSocket::ForwardUp(Ptr<Packet> packet, 
                     Ipv4Header header, 
                     uint16_t port,
                     Ptr<Ipv4Interface> incomingInterface)
{
    NS_LOG_FUNCTION(this << packet << header << port << incomingInterface);

    // 1. 如果接收已关闭，丢弃数据包
    if (m_shutdownRecv)
    {
        return;
    }

    // 2. 如果接收缓冲区有足够的空间，将数据包放入接收队列
    if ((m_rxAvailable + packet->GetSize()) <= m_rcvBufSize)
    {
        Address address = InetSocketAddress(header.GetSource(), port);
        m_deliveryQueue.emplace(packet, address);
        m_rxAvailable += packet->GetSize();
        // 通知应用层有数据可读
        NotifyDataRecv();
    }
    else
    {
        NS_LOG_WARN("No receive buffer space available.  Drop.");
        m_dropTrace(packet);
    }
}

}
