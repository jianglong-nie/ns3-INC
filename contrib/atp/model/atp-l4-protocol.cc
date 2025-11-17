#include "atp-tag.h"
#include "atp-l4-protocol.h"
#include "atp-socket.h"
#include "atp-socket-factory.h"
#include "atp-static-routing.h"

#include "ns3/ipv4-end-point-demux.h"
#include "ns3/ipv4-end-point.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/object-map.h"
#include "ns3/packet.h"

#include <unordered_map>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPL4Protocol");

NS_OBJECT_ENSURE_REGISTERED(ATPL4Protocol);

// TBD: Need to assign a protocol number for ATP
const uint8_t ATPL4Protocol::PROT_NUMBER = 142; 

TypeId
ATPL4Protocol::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ATPL4Protocol")
            .SetParent<IpL4Protocol>()
            .SetGroupName("Internet")
            .AddConstructor<ATPL4Protocol>()
            .AddAttribute("SocketList",
                         "The list of sockets associated to this protocol.",
                         ObjectMapValue(),
                         MakeObjectMapAccessor(&ATPL4Protocol::m_sockets),
                         MakeObjectMapChecker<ATPSocket>())
            .AddTraceSource("Drop",
                           "Trace source indicating a packet has been dropped at L4 layer",
                           MakeTraceSourceAccessor(&ATPL4Protocol::m_dropTrace),
                           "ns3::Packet::TracedCallback");
    return tid;
}

ATPL4Protocol::ATPL4Protocol()
    : m_endPoints(new Ipv4EndPointDemux()),
      m_enableAggregation(false)
{
    NS_LOG_FUNCTION(this);

    m_aggregators.resize(MAX_AGGREGATORS);
}

ATPL4Protocol::~ATPL4Protocol()
{
    NS_LOG_FUNCTION(this);
}

void
ATPL4Protocol::SetNode(Ptr<Node> node)
{
    m_node = node;
}

// 
void
ATPL4Protocol::NotifyNewAggregate()
{
    NS_LOG_FUNCTION(this);
    Ptr<Node> node = this->GetObject<Node>();
    Ptr<Ipv4> ipv4 = this->GetObject<Ipv4>();

    if (!m_node)
    {
        if (node && ipv4)
        {
            this->SetNode(node);
            Ptr<ATPSocketFactory> atpFactory = CreateObject<ATPSocketFactory>();
            atpFactory->SetATP(this);
            node->AggregateObject(atpFactory);
        }
    }

    if (ipv4 && m_downTarget.IsNull())
    {
        ipv4->Insert(this);
        this->SetDownTarget(MakeCallback(&Ipv4::Send, ipv4));
    }
    
    IpL4Protocol::NotifyNewAggregate();
}

int
ATPL4Protocol::GetProtocolNumber() const
{
    return PROT_NUMBER;
}

void
ATPL4Protocol::DoDispose()
{
    NS_LOG_FUNCTION(this);
    for (auto i = m_sockets.begin(); i != m_sockets.end(); i++)
    {
        i->second = nullptr;
    }
    m_sockets.clear();

    if (m_endPoints != nullptr)
    {
        delete m_endPoints;
        m_endPoints = nullptr;
    }
    
    m_node = nullptr;
    m_downTarget.Nullify();
    IpL4Protocol::DoDispose();
}

Ptr<Socket>
ATPL4Protocol::CreateSocket()
{
    NS_LOG_FUNCTION(this);
    Ptr<ATPSocket> socket = CreateObject<ATPSocket>();
    socket->SetNode(m_node);
    socket->SetATP(this);
    m_sockets[m_socketIndex++] = socket;
    return socket;
}

bool
ATPL4Protocol::RemoveSocket(Ptr<ATPSocket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    for (auto& socketItem : m_sockets)
    {
        if (socketItem.second == socket)
        {
            socketItem.second = nullptr;
            m_sockets.erase(socketItem.first);
            return true;
        }
    }
    return false;
}

Ipv4EndPoint*
ATPL4Protocol::Allocate()
{
    NS_LOG_FUNCTION(this);
    return m_endPoints->Allocate();
}

Ipv4EndPoint*
ATPL4Protocol::Allocate(Ipv4Address address)
{
    NS_LOG_FUNCTION(this << address);
    return m_endPoints->Allocate(address);
}

Ipv4EndPoint*
ATPL4Protocol::Allocate(Ptr<NetDevice> boundNetDevice, uint16_t port)
{
    NS_LOG_FUNCTION(this << boundNetDevice << port);
    return m_endPoints->Allocate(boundNetDevice, port);
}

Ipv4EndPoint*
ATPL4Protocol::Allocate(Ptr<NetDevice> boundNetDevice, Ipv4Address address, uint16_t port)
{
    NS_LOG_FUNCTION(this << boundNetDevice << address << port);
    return m_endPoints->Allocate(boundNetDevice, address, port);
}

Ipv4EndPoint*
ATPL4Protocol::Allocate(Ptr<NetDevice> boundNetDevice,
                        Ipv4Address localAddress,
                        uint16_t localPort,
                        Ipv4Address peerAddress,
                        uint16_t peerPort)
{
    NS_LOG_FUNCTION(this << boundNetDevice << localAddress << localPort << peerAddress << peerPort);
    return m_endPoints->Allocate(boundNetDevice, localAddress, localPort, peerAddress, peerPort);
}

void
ATPL4Protocol::DeAllocate(Ipv4EndPoint* endPoint)
{
    NS_LOG_FUNCTION(this << endPoint);
    m_endPoints->DeAllocate(endPoint);
}

void
ATPL4Protocol::SetEnableAggregation(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    m_enableAggregation = enable;
}

void
ATPL4Protocol::SetAggregatorFaninDegree(uint8_t jobId, uint8_t faninDegree)
{
    NS_LOG_FUNCTION(this << faninDegree);
    for (auto& aggregator : m_aggregators)
    {
        aggregator.m_jobFaninDegree[jobId] = faninDegree;
    }
}

void
ATPL4Protocol::SetLayerId(uint8_t layerId)
{
    NS_LOG_FUNCTION(this << static_cast<int>(layerId));
    m_layerId = layerId;
}

uint8_t
ATPL4Protocol::GetLayerId() const
{
    return m_layerId;
}

Ptr<Packet>
ATPL4Protocol::AggregatePacket(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    
    ATPTag atpTag;
    packet->PeekPacketTag(atpTag);

    NS_LOG_INFO("ATP DATA packet with jobId = " << static_cast<int>(atpTag.GetJobId())
                                               << ", seqNum = " << static_cast<int>(atpTag.GetSeqNumber())
                                               << ", workerId = " << static_cast<int>(atpTag.GetWorkerId())
                                               << ", resend = " << static_cast<int>(atpTag.GetResend())
                                               << ", layerId = " << static_cast<int>(m_layerId));

    // 使用分层哈希函数计算索引，避免跨层连环冲突
    std::size_t index = Aggregator::HashToIndexLayer(atpTag.GetJobId(), atpTag.GetSeqNumber(), m_layerId, MAX_AGGREGATORS);
    
    // 获取对应的聚合器
    Aggregator& aggregator = m_aggregators[index];

    // 重传包处理逻辑
    if (atpTag.GetResend() == 1)
    {
        // aggregator为空，直接发走
        if (aggregator.IsEmpty()) {
            NS_LOG_INFO("Retransmit packet: aggregator empty, send directly");
            return packet;
        }
        
        // jobId或seqNum不匹配，直接发走
        if (aggregator.m_jobId != atpTag.GetJobId() || aggregator.m_seqNum != atpTag.GetSeqNumber()) {
            NS_LOG_INFO("Retransmit packet: jobId/seqNum mismatch, send directly");
            return packet;
        }
        
        // 检查bitmap overlap
        uint8_t workerId = atpTag.GetWorkerId();
        if ((workerId & aggregator.m_workerIdAgg) != 0) {
            NS_LOG_INFO("Retransmit packet: already aggregated (overlap), send directly");
            return packet;
        }
        
        // 未聚合过，参与聚合并直接发走
        NS_LOG_INFO("Retransmit packet: not aggregated, merge and send");
        aggregator.m_workerIdAgg |= workerId;
        
        // 更新tag并返回聚合后的包
        ATPTag newTag;
        aggregator.m_packet->PeekPacketTag(newTag);
        aggregator.m_packet->RemovePacketTag(newTag);
        newTag.SetWorkerId(aggregator.m_workerIdAgg);
        aggregator.m_packet->AddPacketTag(newTag);
        
        return aggregator.m_packet->Copy();
    }

    // 正常包处理逻辑
    if (aggregator.IsEmpty() || 
        (aggregator.m_jobId == atpTag.GetJobId() && 
         aggregator.m_seqNum == atpTag.GetSeqNumber()))
    {
        // 如果是空的，初始化jobId和seqNum
        if (aggregator.IsEmpty()) {
            aggregator.m_jobId = atpTag.GetJobId();
            aggregator.m_seqNum = atpTag.GetSeqNumber();
        }

        // 添加数据包到聚合器
        bool aggregationComplete = aggregator.AddPacket(packet->Copy());
        if (aggregationComplete)
        {
            // 聚合完成，发送聚合后的数据包
            Ptr<Packet> aggregatedPacket = aggregator.GetAggregatedPacket();

            ATPTag newTag;
            aggregatedPacket->PeekPacketTag(newTag);
            aggregatedPacket->RemovePacketTag(newTag);

            // 改成聚合完成标志AGG
            if (aggregator.m_jobFaninDegree[newTag.GetJobId()] == newTag.GetFaninDegree())
            {
                newTag.SetPacketType(ATPTag::AGG);
            }
            aggregatedPacket->AddPacketTag(newTag);

            ATPTag aggcompletag;
            aggregatedPacket->PeekPacketTag(aggcompletag);

            NS_LOG_INFO("Aggregated complete, packet sent with jobId = " << static_cast<int>(aggcompletag.GetJobId())
            << ", seqNum = " << static_cast<int>(aggcompletag.GetSeqNumber()) << ", workerid = " << static_cast<int>(aggcompletag.GetWorkerId()));


            return aggregatedPacket;
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        NS_LOG_INFO("ATP DATA packet with jobId = " << static_cast<int>(atpTag.GetJobId())
        << ", seqNum = " << static_cast<int>(atpTag.GetSeqNumber())
        << ", workerId = " << static_cast<int>(atpTag.GetWorkerId())
        << ", layerId = " << static_cast<int>(m_layerId) << " detected hash collision. Returning original packet.");
        return packet;
    }
}

Ptr<Packet>
ATPL4Protocol::AggregateStart(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    ATPTag atpTag;
    bool hasATPTag = packet->PeekPacketTag(atpTag);
    Ptr<Packet> aggregatedPacket = nullptr;

    if (hasATPTag && atpTag.GetPacketType() == ATPTag::DATA)
    {
        aggregatedPacket = AggregatePacket(packet->Copy());

        if (aggregatedPacket == nullptr)
        {
            NS_LOG_INFO("Packet is waiting for aggregation.");
        }
    }
    else if (hasATPTag && atpTag.GetPacketType() == ATPTag::ACK)
    {
        NS_LOG_INFO("ATPL4Protocol: Reset aggregator for jobId " << static_cast<int>(atpTag.GetJobId())
                                               << " seqNum " << static_cast<int>(atpTag.GetSeqNumber()));
        // 使用分层哈希函数计算索引，与聚合时使用相同的哈希方式
        std::size_t index = Aggregator::HashToIndexLayer(atpTag.GetJobId(), atpTag.GetSeqNumber(), m_layerId, MAX_AGGREGATORS);
    
        // 获取对应的聚合器
        Aggregator& aggregator = m_aggregators[index];

        aggregator.Reset();
        aggregatedPacket = packet->Copy();

    }
    else
    {
        aggregatedPacket = packet->Copy();
    }

    return aggregatedPacket;
}

// 需要做相关修改
IpL4Protocol::RxStatus
ATPL4Protocol::Receive(Ptr<Packet> packet, const Ipv4Header& header, Ptr<Ipv4Interface> interface)
{
    NS_LOG_FUNCTION(this << packet << header);
    ATPTag atpTag;

    // 只是peek ATP头部
    packet->PeekPacketTag(atpTag);

    // 查找匹配的端点
    NS_LOG_DEBUG("Looking up dst " << header.GetDestination() << " port "
                                  << atpTag.GetDestinationPort());
    Ipv4EndPointDemux::EndPoints endPoints = m_endPoints->Lookup(header.GetDestination(),
                                                                atpTag.GetDestinationPort(),
                                                                header.GetSource(),
                                                                atpTag.GetSourcePort(),
                                                                interface);
    if (endPoints.empty())
    {
        NS_LOG_LOGIC("RX_ENDPOINT_UNREACH");
        m_dropTrace(packet, "No matching endpoint");
        return IpL4Protocol::RX_ENDPOINT_UNREACH;
    }

    
    // 将数据包转发给所有匹配的端点
    NS_ASSERT_MSG(endPoints.size() == 1, "ATP expects exactly one endpoint");
    NS_LOG_LOGIC("ATPL4Protocol " << this
                                  << " received a packet and"
                                     " now forwarding it up to endpoint/socket");

    (*endPoints.begin())->ForwardUp(packet, header, atpTag.GetSourcePort(), interface);

    return IpL4Protocol::RX_OK;
}

IpL4Protocol::RxStatus
ATPL4Protocol::Receive(Ptr<Packet> packet,
                       const Ipv6Header& header,
                       Ptr<Ipv6Interface> interface)
{
    NS_LOG_FUNCTION(this << packet << header);
    return IpL4Protocol::RX_ENDPOINT_UNREACH;
}

void
ATPL4Protocol::Send(Ptr<Packet> packet,
                   Ipv4Address saddr,
                   Ipv4Address daddr,
                   uint16_t sport,
                   uint16_t dport)
{
    NS_LOG_FUNCTION(this << packet << saddr << daddr << sport << dport);

    // 1. 提取packet中的atptag
    ATPTag initTag;
    
    bool hasATPTag = packet->PeekPacketTag(initTag);
    packet->RemovePacketTag(initTag);

    if (hasATPTag)
    {
        // 设置Tag
        NS_LOG_INFO("Get ATPTag from packet");
        ATPTag atpTag;

        // 使用CopyFrom函数复制所有值
        atpTag.CopyFrom(initTag);
        
        // 更新端口信息
        atpTag.SetSourcePort(sport);
        atpTag.SetDestinationPort(dport);

        packet->AddPacketTag(atpTag);

        // 发送数据包
        m_downTarget(packet, saddr, daddr, PROT_NUMBER, nullptr);
    }
    else
    {
        NS_LOG_INFO("No ATPTag in packet");
        m_downTarget(packet, saddr, daddr, PROT_NUMBER, nullptr);
    }
}

void
ATPL4Protocol::Send(Ptr<Packet> packet,
                   Ipv4Address saddr,
                   Ipv4Address daddr,
                   uint16_t sport,
                   uint16_t dport,
                   Ptr<Ipv4Route> route)
{
    NS_LOG_FUNCTION(this << packet << saddr << daddr << sport << dport << route);

    
    // 1. 提取packet中的atptag
    ATPTag initTag;

    bool hasATPTag = packet->PeekPacketTag(initTag);
    packet->RemovePacketTag(initTag);

    if (hasATPTag)
    {
        // 设置Tag
        NS_LOG_INFO("Get ATPTag from packet");
        ATPTag atpTag;
        
        // 使用CopyFrom函数复制所有值
        atpTag.CopyFrom(initTag);

        // 更新端口信息
        atpTag.SetDestinationPort(dport);
        atpTag.SetSourcePort(sport);
        
        packet->AddPacketTag(atpTag);

        // 4. 发送数据包
        m_downTarget(packet, saddr, daddr, PROT_NUMBER, route);
    }
    else
    {
        NS_LOG_INFO("No ATPTag in packet");
        m_downTarget(packet, saddr, daddr, PROT_NUMBER, route);
    }
}

void
ATPL4Protocol::SetDownTarget(IpL4Protocol::DownTargetCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_downTarget = callback;
}

void
ATPL4Protocol::SetDownTarget6(IpL4Protocol::DownTargetCallback6 callback)
{
    NS_LOG_FUNCTION(this);
    m_downTarget6 = callback;
}

IpL4Protocol::DownTargetCallback
ATPL4Protocol::GetDownTarget() const
{
    return m_downTarget;
}

IpL4Protocol::DownTargetCallback6
ATPL4Protocol::GetDownTarget6() const
{
    return m_downTarget6;
}

} // namespace ns3
