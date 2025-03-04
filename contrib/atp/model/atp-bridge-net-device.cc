/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Gustavo Carneiro  <gjc@inescporto.pt>
 */
#include "atp-bridge-net-device.h"

#include "ns3/boolean.h"
#include "ns3/channel.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/inet-socket-address.h"

/**
 * \file
 * \ingroup bridge
 * ns3::BridgeNetDevice implementation.
 */

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPBridgeNetDevice");

NS_OBJECT_ENSURE_REGISTERED(ATPBridgeNetDevice);

Aggregator::Aggregator()
    : m_jobId(0),
      m_seqNum(0),
      m_count(0),
      m_faninDegree(2),
      m_packet(nullptr)
{
    NS_LOG_FUNCTION(this);
}

Aggregator::~Aggregator()
{
    NS_LOG_FUNCTION(this);
    
    // 如果m_packet不为空，释放它
    if (m_packet != nullptr)
    {
        m_packet = nullptr;
    }
}

void
Aggregator::SetFaninDegree(uint32_t faninDegree)
{
    NS_LOG_FUNCTION(this << faninDegree);
    m_faninDegree = faninDegree;
}

bool
Aggregator::AddPacket(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    m_packet = packet->Copy();

    m_count++;
    
    // 检查是否完成聚合
    if (m_count >= m_faninDegree)
    {   
        return true;  // 聚合完成，可以取出数据包了
    }
    
    return false;  // 聚合未完成，继续收集数据包
}

Ptr<Packet>
Aggregator::GetAggregatedPacket() const
{
    NS_LOG_FUNCTION(this);
    return m_packet;
}

void
Aggregator::Reset()
{
    NS_LOG_FUNCTION(this);
    m_jobId = 0;
    m_seqNum = 0;
    m_count = 0;
    m_packet = nullptr;
}


void
ATPBridgeNetDevice::AggregatePacket(Ptr<NetDevice> incomingPort,
                                Ptr<const Packet> packet,
                                uint16_t protocol,
                                Mac48Address src,
                                Mac48Address dst)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_DEBUG("AggregatePacket (incomingPort="
                 << incomingPort->GetInstanceTypeId().GetName() << ", packet=" << packet
                 << ", protocol=" << protocol << ", src=" << src << ", dst=" << dst << ")");

    // 检查数据包是否包含ATP标签
    ATPTag atpTag;
    if (!packet->PeekPacketTag(atpTag))
    {
        // 如果不是ATP数据包，直接转发
        NS_LOG_INFO("Not ATP packet, forward directly");
        ForwardUnicast(incomingPort, packet, protocol, src, dst);
        return;
    }
    NS_LOG_INFO("ATP packet, store and aggregate");

    uint8_t jobId = atpTag.GetJobId();
    uint8_t seqNum = atpTag.GetSeqNumber();

    // 查找匹配的聚合器
    auto matchedIt = m_aggregators.end();
    for (auto it = m_aggregators.begin(); it != m_aggregators.end(); ++it)
    {
        if (it->m_jobId == jobId && it->m_seqNum == seqNum)
        {
            matchedIt = it;
            break;
        }
    }

    if (matchedIt == m_aggregators.end())
    {
        // 如果没找到匹配的聚合器且未达到上限，创建新的
        if (m_aggregators.size() < MAX_AGGREGATORS)
        {
            // 创建新的聚合器并设置JobId和SeqNum
            m_aggregators.emplace_back();
            matchedIt = m_aggregators.end() - 1;
            matchedIt->m_jobId = jobId;
            matchedIt->m_seqNum = seqNum;
        }
        else
        {
            // 达到上限，丢弃数据包
            NS_LOG_WARN("Aggregator buffer full, packet dropped");
            return;
        }
    }

    // 添加数据包到聚合器
    bool aggregationComplete = matchedIt->AddPacket(packet);
    if (aggregationComplete)
    {
        // 达到聚合条件，获取聚合后的数据包

        NS_LOG_INFO("ATPBridgeNetDevice AggregatePacket: Aggregation complete");

        // 重新添加修改后的头部
        Ptr<Packet> aggregatedPacket = matchedIt->GetAggregatedPacket();

        ATPHeader atpHeader;
        aggregatedPacket->RemoveHeader(atpHeader);

        // 保留原有标志，添加AGGREGATED标志
        //atpHeader.SetPacketType(ATPHeader::AGGREGATED);

        aggregatedPacket->AddHeader(atpHeader);

        NS_LOG_INFO("ATPBridgeNetDevice AggregatePacket: PacketType=0x" << std::hex << static_cast<int>(atpHeader.GetPacketType()) << std::dec);  

        // 转发聚合后的数据包，使用原始的MAC地址
        ForwardUnicast(incomingPort, aggregatedPacket, protocol, src, dst);

        // 重置聚合器
        matchedIt->Reset();
    }
}


TypeId
ATPBridgeNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ATPBridgeNetDevice")
            .SetParent<NetDevice>()
            .SetGroupName("Bridge")
            .AddConstructor<ATPBridgeNetDevice>()
            .AddAttribute("Mtu",
                          "The MAC-level Maximum Transmission Unit",
                          UintegerValue(1500),
                          MakeUintegerAccessor(&ATPBridgeNetDevice::SetMtu, &ATPBridgeNetDevice::GetMtu),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("EnableLearning",
                          "Enable the learning mode of the Learning Bridge",
                          BooleanValue(true),
                          MakeBooleanAccessor(&ATPBridgeNetDevice::m_enableLearning),
                          MakeBooleanChecker())
            .AddAttribute("ExpirationTime",
                          "Time it takes for learned MAC state entry to expire.",
                          TimeValue(Seconds(300)),
                          MakeTimeAccessor(&ATPBridgeNetDevice::m_expirationTime),
                          MakeTimeChecker());
    return tid;
}

ATPBridgeNetDevice::ATPBridgeNetDevice()
    : m_node(nullptr),
      m_ifIndex(0)
{
    NS_LOG_FUNCTION_NOARGS();
    m_channel = CreateObject<ATPBridgeChannel>();
}

ATPBridgeNetDevice::~ATPBridgeNetDevice()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
ATPBridgeNetDevice::DoDispose()
{
    NS_LOG_FUNCTION_NOARGS();
    for (auto iter = m_ports.begin(); iter != m_ports.end(); iter++)
    {
        *iter = nullptr;
    }
    m_ports.clear();
    m_channel = nullptr;
    m_node = nullptr;
    NetDevice::DoDispose();
}

void
ATPBridgeNetDevice::ReceiveFromDevice(Ptr<NetDevice> incomingPort,
                                   Ptr<const Packet> packet,
                                   uint16_t protocol,
                                   const Address& src,
                                   const Address& dst,
                                   PacketType packetType)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_DEBUG("UID is " << packet->GetUid());

    Mac48Address src48 = Mac48Address::ConvertFrom(src);
    Mac48Address dst48 = Mac48Address::ConvertFrom(dst);

    if (!m_promiscRxCallback.IsNull())
    {
        m_promiscRxCallback(this, packet, protocol, src, dst, packetType);
    }

    switch (packetType)
    {
    case PACKET_HOST:
        if (dst48 == m_address)
        {
            Learn(src48, incomingPort);
            m_rxCallback(this, packet, protocol, src);
        }
        break;

    case PACKET_BROADCAST:
    case PACKET_MULTICAST:
        m_rxCallback(this, packet, protocol, src);
        ForwardBroadcast(incomingPort, packet, protocol, src48, dst48);
        break;

    case PACKET_OTHERHOST:
        if (dst48 == m_address)
        {
            Learn(src48, incomingPort);
            m_rxCallback(this, packet, protocol, src);
        }
        else
        {
            //ForwardUnicast(incomingPort, packet, protocol, src48, dst48);
            AggregatePacket(incomingPort, packet, protocol, src48, dst48);
        }
        break;
    }
}

void
ATPBridgeNetDevice::ForwardUnicast(Ptr<NetDevice> incomingPort,
                                Ptr<const Packet> packet,
                                uint16_t protocol,
                                Mac48Address src,
                                Mac48Address dst)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_DEBUG("LearningBridgeForward (incomingPort="
                 << incomingPort->GetInstanceTypeId().GetName() << ", packet=" << packet
                 << ", protocol=" << protocol << ", src=" << src << ", dst=" << dst << ")");

    Learn(src, incomingPort);
    Ptr<NetDevice> outPort = GetLearnedState(dst);
    if (outPort && outPort != incomingPort)
    {
        NS_LOG_LOGIC("Learning bridge state says to use port `"
                     << outPort->GetInstanceTypeId().GetName() << "'");
        outPort->SendFrom(packet->Copy(), src, dst, protocol);
    }
    else
    {
        NS_LOG_LOGIC("No learned state: send through all ports");
        for (auto iter = m_ports.begin(); iter != m_ports.end(); iter++)
        {
            Ptr<NetDevice> port = *iter;
            if (port != incomingPort)
            {
                NS_LOG_LOGIC("LearningBridgeForward ("
                             << src << " => " << dst
                             << "): " << incomingPort->GetInstanceTypeId().GetName() << " --> "
                             << port->GetInstanceTypeId().GetName() << " (UID " << packet->GetUid()
                             << ").");
                port->SendFrom(packet->Copy(), src, dst, protocol);
            }
        }
    }
}

void
ATPBridgeNetDevice::ForwardBroadcast(Ptr<NetDevice> incomingPort,
                                  Ptr<const Packet> packet,
                                  uint16_t protocol,
                                  Mac48Address src,
                                  Mac48Address dst)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_DEBUG("LearningBridgeForward (incomingPort="
                 << incomingPort->GetInstanceTypeId().GetName() << ", packet=" << packet
                 << ", protocol=" << protocol << ", src=" << src << ", dst=" << dst << ")");
    Learn(src, incomingPort);

    for (auto iter = m_ports.begin(); iter != m_ports.end(); iter++)
    {
        Ptr<NetDevice> port = *iter;
        if (port != incomingPort)
        {
            NS_LOG_LOGIC("LearningBridgeForward (" << src << " => " << dst << "): "
                                                   << incomingPort->GetInstanceTypeId().GetName()
                                                   << " --> " << port->GetInstanceTypeId().GetName()
                                                   << " (UID " << packet->GetUid() << ").");
            port->SendFrom(packet->Copy(), src, dst, protocol);
        }
    }
}

void
ATPBridgeNetDevice::Learn(Mac48Address source, Ptr<NetDevice> port)
{
    NS_LOG_FUNCTION_NOARGS();
    if (m_enableLearning)
    {
        LearnedState& state = m_learnState[source];
        state.associatedPort = port;
        state.expirationTime = Simulator::Now() + m_expirationTime;
    }
}

Ptr<NetDevice>
ATPBridgeNetDevice::GetLearnedState(Mac48Address source)
{
    NS_LOG_FUNCTION_NOARGS();
    if (m_enableLearning)
    {
        Time now = Simulator::Now();
        auto iter = m_learnState.find(source);
        if (iter != m_learnState.end())
        {
            LearnedState& state = iter->second;
            if (state.expirationTime > now)
            {
                return state.associatedPort;
            }
            else
            {
                m_learnState.erase(iter);
            }
        }
    }
    return nullptr;
}

uint32_t
ATPBridgeNetDevice::GetNBridgePorts() const
{
    NS_LOG_FUNCTION_NOARGS();
    return m_ports.size();
}

Ptr<NetDevice>
ATPBridgeNetDevice::GetBridgePort(uint32_t n) const
{
    NS_LOG_FUNCTION_NOARGS();
    return m_ports[n];
}

void
ATPBridgeNetDevice::AddBridgePort(Ptr<NetDevice> bridgePort)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_ASSERT(bridgePort != this);
    if (!Mac48Address::IsMatchingType(bridgePort->GetAddress()))
    {
        NS_FATAL_ERROR("Device does not support eui 48 addresses: cannot be added to bridge.");
    }
    if (!bridgePort->SupportsSendFrom())
    {
        NS_FATAL_ERROR("Device does not support SendFrom: cannot be added to bridge.");
    }
    if (m_address == Mac48Address())
    {
        m_address = Mac48Address::ConvertFrom(bridgePort->GetAddress());
    }

    NS_LOG_DEBUG("RegisterProtocolHandler for " << bridgePort->GetInstanceTypeId().GetName());
    m_node->RegisterProtocolHandler(MakeCallback(&ATPBridgeNetDevice::ReceiveFromDevice, this),
                                    0,
                                    bridgePort,
                                    true);
    m_ports.push_back(bridgePort);
    m_channel->AddChannel(bridgePort->GetChannel());
}

void
ATPBridgeNetDevice::SetIfIndex(const uint32_t index)
{
    NS_LOG_FUNCTION_NOARGS();
    m_ifIndex = index;
}

uint32_t
ATPBridgeNetDevice::GetIfIndex() const
{
    NS_LOG_FUNCTION_NOARGS();
    return m_ifIndex;
}

Ptr<Channel>
ATPBridgeNetDevice::GetChannel() const
{
    NS_LOG_FUNCTION_NOARGS();
    return m_channel;
}

void
ATPBridgeNetDevice::SetAddress(Address address)
{
    NS_LOG_FUNCTION_NOARGS();
    m_address = Mac48Address::ConvertFrom(address);
}

Address
ATPBridgeNetDevice::GetAddress() const
{
    NS_LOG_FUNCTION_NOARGS();
    return m_address;
}

bool
ATPBridgeNetDevice::SetMtu(const uint16_t mtu)
{
    NS_LOG_FUNCTION_NOARGS();
    m_mtu = mtu;
    return true;
}

uint16_t
ATPBridgeNetDevice::GetMtu() const
{
    NS_LOG_FUNCTION_NOARGS();
    return m_mtu;
}

bool
ATPBridgeNetDevice::IsLinkUp() const
{
    NS_LOG_FUNCTION_NOARGS();
    return true;
}

void
ATPBridgeNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
}

bool
ATPBridgeNetDevice::IsBroadcast() const
{
    NS_LOG_FUNCTION_NOARGS();
    return true;
}

Address
ATPBridgeNetDevice::GetBroadcast() const
{
    NS_LOG_FUNCTION_NOARGS();
    return Mac48Address::GetBroadcast();
}

bool
ATPBridgeNetDevice::IsMulticast() const
{
    NS_LOG_FUNCTION_NOARGS();
    return true;
}

Address
ATPBridgeNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    NS_LOG_FUNCTION(this << multicastGroup);
    Mac48Address multicast = Mac48Address::GetMulticast(multicastGroup);
    return multicast;
}

bool
ATPBridgeNetDevice::IsPointToPoint() const
{
    NS_LOG_FUNCTION_NOARGS();
    return false;
}

bool
ATPBridgeNetDevice::IsBridge() const
{
    NS_LOG_FUNCTION_NOARGS();
    return true;
}

bool
ATPBridgeNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION_NOARGS();
    return SendFrom(packet, m_address, dest, protocolNumber);
}

bool
ATPBridgeNetDevice::SendFrom(Ptr<Packet> packet,
                          const Address& src,
                          const Address& dest,
                          uint16_t protocolNumber)
{
    NS_LOG_FUNCTION_NOARGS();
    Mac48Address dst = Mac48Address::ConvertFrom(dest);

    // try to use the learned state if data is unicast
    if (!dst.IsGroup())
    {
        Ptr<NetDevice> outPort = GetLearnedState(dst);
        if (outPort)
        {
            outPort->SendFrom(packet, src, dest, protocolNumber);
            return true;
        }
    }

    // data was not unicast or no state has been learned for that mac
    // address => flood through all ports.
    Ptr<Packet> pktCopy;
    for (auto iter = m_ports.begin(); iter != m_ports.end(); iter++)
    {
        pktCopy = packet->Copy();
        Ptr<NetDevice> port = *iter;
        port->SendFrom(pktCopy, src, dest, protocolNumber);
    }

    return true;
}

Ptr<Node>
ATPBridgeNetDevice::GetNode() const
{
    NS_LOG_FUNCTION_NOARGS();
    return m_node;
}

void
ATPBridgeNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION_NOARGS();
    m_node = node;
}

bool
ATPBridgeNetDevice::NeedsArp() const
{
    NS_LOG_FUNCTION_NOARGS();
    return true;
}

void
ATPBridgeNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
{
    NS_LOG_FUNCTION_NOARGS();
    m_rxCallback = cb;
}

void
ATPBridgeNetDevice::SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback cb)
{
    NS_LOG_FUNCTION_NOARGS();
    m_promiscRxCallback = cb;
}

bool
ATPBridgeNetDevice::SupportsSendFrom() const
{
    NS_LOG_FUNCTION_NOARGS();
    return true;
}

Address
ATPBridgeNetDevice::GetMulticast(Ipv6Address addr) const
{
    NS_LOG_FUNCTION(this << addr);
    return Mac48Address::GetMulticast(addr);
}

} // namespace ns3
