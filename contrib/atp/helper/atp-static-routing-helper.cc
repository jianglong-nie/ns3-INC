/*
 * Copyright (c) 2009 University of Washington
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "atp-static-routing-helper.h"

#include "ns3/assert.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/node.h"
#include "ns3/ptr.h"

#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPStaticRoutingHelper");

ATPStaticRoutingHelper::ATPStaticRoutingHelper()
{
}

ATPStaticRoutingHelper::ATPStaticRoutingHelper(const ATPStaticRoutingHelper& o)
{
}

ATPStaticRoutingHelper*
ATPStaticRoutingHelper::Copy() const
{
    return new ATPStaticRoutingHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
ATPStaticRoutingHelper::Create(Ptr<Node> node) const
{
    return CreateObject<ATPStaticRouting>();
}

Ptr<ATPStaticRouting>
ATPStaticRoutingHelper::GetStaticRouting(Ptr<Ipv4> ipv4) const
{
    NS_LOG_FUNCTION(this);
    Ptr<Ipv4RoutingProtocol> ipv4rp = ipv4->GetRoutingProtocol();
    NS_ASSERT_MSG(ipv4rp, "No routing protocol associated with Ipv4");
    if (DynamicCast<ATPStaticRouting>(ipv4rp))
    {
        NS_LOG_LOGIC("Static routing found as the main IPv4 routing protocol.");
        return DynamicCast<ATPStaticRouting>(ipv4rp);
    }
    if (DynamicCast<Ipv4ListRouting>(ipv4rp))
    {
        Ptr<Ipv4ListRouting> lrp = DynamicCast<Ipv4ListRouting>(ipv4rp);
        int16_t priority;
        for (uint32_t i = 0; i < lrp->GetNRoutingProtocols(); i++)
        {
            NS_LOG_LOGIC("Searching for static routing in list");
            Ptr<Ipv4RoutingProtocol> temp = lrp->GetRoutingProtocol(i, priority);
            if (DynamicCast<ATPStaticRouting>(temp))
            {
                NS_LOG_LOGIC("Found static routing in list");
                return DynamicCast<ATPStaticRouting>(temp);
            }
        }
    }
    NS_LOG_LOGIC("Static routing not found");
    return nullptr;
}

void
ATPStaticRoutingHelper::AddMulticastRoute(Ptr<Node> n,
                                           Ipv4Address source,
                                           Ipv4Address group,
                                           Ptr<NetDevice> input,
                                           NetDeviceContainer output)
{
    Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();

    // We need to convert the NetDeviceContainer to an array of interface
    // numbers
    std::vector<uint32_t> outputInterfaces;
    for (auto i = output.Begin(); i != output.End(); ++i)
    {
        Ptr<NetDevice> nd = *i;
        int32_t interface = ipv4->GetInterfaceForDevice(nd);
        NS_ASSERT_MSG(interface >= 0,
                      "ATPStaticRoutingHelper::AddMulticastRoute(): "
                      "Expected an interface associated with the device nd");
        outputInterfaces.push_back(interface);
    }

    int32_t inputInterface = ipv4->GetInterfaceForDevice(input);
    NS_ASSERT_MSG(inputInterface >= 0,
                  "ATPStaticRoutingHelper::AddMulticastRoute(): "
                  "Expected an interface associated with the device input");
    ATPStaticRoutingHelper helper;
    Ptr<ATPStaticRouting> atpStaticRouting = helper.GetStaticRouting(ipv4);
    if (!atpStaticRouting)
    {
        NS_ASSERT_MSG(atpStaticRouting,
                      "ATPStaticRoutingHelper::AddMulticastRoute(): "
                      "Expected an ATPStaticRouting associated with this node");
    }
    atpStaticRouting->AddMulticastRoute(source, group, inputInterface, outputInterfaces);
}

void
ATPStaticRoutingHelper::AddMulticastRoute(Ptr<Node> n,
                                           Ipv4Address source,
                                           Ipv4Address group,
                                           std::string inputName,
                                           NetDeviceContainer output)
{
    Ptr<NetDevice> input = Names::Find<NetDevice>(inputName);
    AddMulticastRoute(n, source, group, input, output);
}

void
ATPStaticRoutingHelper::AddMulticastRoute(std::string nName,
                                           Ipv4Address source,
                                           Ipv4Address group,
                                           Ptr<NetDevice> input,
                                           NetDeviceContainer output)
{
    Ptr<Node> n = Names::Find<Node>(nName);
    AddMulticastRoute(n, source, group, input, output);
}

void
ATPStaticRoutingHelper::AddMulticastRoute(std::string nName,
                                           Ipv4Address source,
                                           Ipv4Address group,
                                           std::string inputName,
                                           NetDeviceContainer output)
{
    Ptr<NetDevice> input = Names::Find<NetDevice>(inputName);
    Ptr<Node> n = Names::Find<Node>(nName);
    AddMulticastRoute(n, source, group, input, output);
}

void
ATPStaticRoutingHelper::SetDefaultMulticastRoute(Ptr<Node> n, Ptr<NetDevice> nd)
{
    Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
    int32_t interfaceSrc = ipv4->GetInterfaceForDevice(nd);
    NS_ASSERT_MSG(interfaceSrc >= 0,
                  "ATPStaticRoutingHelper::SetDefaultMulticastRoute(): "
                  "Expected an interface associated with the device");
    ATPStaticRoutingHelper helper;
    Ptr<ATPStaticRouting> atpStaticRouting = helper.GetStaticRouting(ipv4);
    if (!atpStaticRouting)
    {
        NS_ASSERT_MSG(atpStaticRouting,
                      "ATPStaticRoutingHelper::SetDefaultMulticastRoute(): "
                      "Expected an ATPStaticRouting associated with this node");
    }
    atpStaticRouting->SetDefaultMulticastRoute(interfaceSrc);
}

void
ATPStaticRoutingHelper::SetDefaultMulticastRoute(Ptr<Node> n, std::string ndName)
{
    Ptr<NetDevice> nd = Names::Find<NetDevice>(ndName);
    SetDefaultMulticastRoute(n, nd);
}

void
ATPStaticRoutingHelper::SetDefaultMulticastRoute(std::string nName, Ptr<NetDevice> nd)
{
    Ptr<Node> n = Names::Find<Node>(nName);
    SetDefaultMulticastRoute(n, nd);
}

void
ATPStaticRoutingHelper::SetDefaultMulticastRoute(std::string nName, std::string ndName)
{
    Ptr<Node> n = Names::Find<Node>(nName);
    Ptr<NetDevice> nd = Names::Find<NetDevice>(ndName);
    SetDefaultMulticastRoute(n, nd);
}

} // namespace ns3
