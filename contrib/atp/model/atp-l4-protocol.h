#ifndef ATP_L4_PROTOCOL_H
#define ATP_L4_PROTOCOL_H

#include "ns3/ip-l4-protocol.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "atp-aggregator.h"

#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace ns3
{

class ATPTag;
class Node;
class Socket;
class ATPSocket;
class Ipv4EndPoint;
class Ipv4EndPointDemux;
class Ipv6EndPoint;
class Ipv6EndPointDemux;
class NetDevice;

/**
 * \ingroup atp
 * \brief Implementation of the ATP protocol
 */
class ATPL4Protocol : public IpL4Protocol
{
  public:
    static TypeId GetTypeId();
    static const uint8_t PROT_NUMBER; //!< protocol number (0xFE)

    ATPL4Protocol();
    ~ATPL4Protocol() override;

    // Delete copy constructor and assignment operator to avoid misuse
    ATPL4Protocol(const ATPL4Protocol&) = delete;
    ATPL4Protocol& operator=(const ATPL4Protocol&) = delete;

    void SetNode(Ptr<Node> node);
    int GetProtocolNumber() const override;
    Ptr<Socket> CreateSocket();
    bool RemoveSocket(Ptr<ATPSocket> socket);

    Ipv4EndPoint* Allocate();
    Ipv4EndPoint* Allocate(Ipv4Address address);
    Ipv4EndPoint* Allocate(Ptr<NetDevice> boundNetDevice, uint16_t port);
    Ipv4EndPoint* Allocate(Ptr<NetDevice> boundNetDevice, Ipv4Address address, uint16_t port);
    Ipv4EndPoint* Allocate(Ptr<NetDevice> boundNetDevice,
                          Ipv4Address localAddress,
                          uint16_t localPort,
                          Ipv4Address peerAddress,
                          uint16_t peerPort);

    void DeAllocate(Ipv4EndPoint* endPoint);

    void Send(Ptr<Packet> packet,
             Ipv4Address saddr,
             Ipv4Address daddr,
             uint16_t sport,
             uint16_t dport);
    void Send(Ptr<Packet> packet,
             Ipv4Address saddr,
             Ipv4Address daddr,
             uint16_t sport,
             uint16_t dport,
             Ptr<Ipv4Route> route);

    // From IpL4Protocol
    IpL4Protocol::RxStatus Receive(Ptr<Packet> p,
                                 const Ipv4Header& header,
                                 Ptr<Ipv4Interface> interface) override;
    
    IpL4Protocol::RxStatus Receive(Ptr<Packet> p,
                                 const Ipv6Header& header,
                                 Ptr<Ipv6Interface> interface) override;

    void SetDownTarget(IpL4Protocol::DownTargetCallback cb) override;
    void SetDownTarget6(IpL4Protocol::DownTargetCallback6 cb) override;
    IpL4Protocol::DownTargetCallback GetDownTarget() const override;
    IpL4Protocol::DownTargetCallback6 GetDownTarget6() const override;
    
    void SetEnableAggregation(bool enable);
    Ptr<Packet> AggregatePacket(Ptr<Packet> packet);
    Ptr<Packet> AggregateStart(Ptr<Packet> packet);
    void SetAggregatorFaninDegree(uint8_t jobId, uint8_t faninDegree);
  
  protected:
    void DoDispose() override;
    void NotifyNewAggregate() override;

  private:
    Ptr<Node> m_node;                //!< The node this stack is associated with
    Ipv4EndPointDemux* m_endPoints;  //!< A list of IPv4 end points.

    std::unordered_map<uint64_t, Ptr<ATPSocket>>
        m_sockets;             //!< Unordered map of socket IDs and corresponding sockets
    uint64_t m_socketIndex{0}; //!< Index of the next socket to be created
    IpL4Protocol::DownTargetCallback m_downTarget;   //!< Callback to send packets over IPv4
    IpL4Protocol::DownTargetCallback6 m_downTarget6; //!< Callback to send packets over IPv6

    // 聚合器相关
    bool m_enableAggregation;
    static const uint32_t MAX_AGGREGATORS = 128;  //!< Maximum number of aggregators
    std::vector<Aggregator> m_aggregators;         //!< Vector of aggregators

    /**
     * Trace source for packets dropped at L4 layer
     */
    TracedCallback<Ptr<const Packet>, const char *> m_dropTrace;
};

} // namespace ns3

#endif /* ATP_L4_PROTOCOL_H */