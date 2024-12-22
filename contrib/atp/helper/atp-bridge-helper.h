/*
 * Copyright (c) 2008 INRIA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Author: Gustavo Carneiro <gjc@inescporto.pt>
 */
#ifndef ATP_BRIDGE_HELPER_H
#define ATP_BRIDGE_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"

#include <string>

/**
 * \file
 * \ingroup bridge
 * ns3::ATPBridgeHelper declaration.
 */

namespace ns3
{

class Node;
class AttributeValue;

/**
 * \ingroup bridge
 * \brief Add capability to bridge multiple LAN segments (IEEE 802.1D bridging)
 */
class ATPBridgeHelper
{
  public:
    /*
     * Construct a ATPBridgeHelper
     */
    ATPBridgeHelper();
    /**
     * Set an attribute on each ns3::ATPBridgeNetDevice created by
     * ATPBridgeHelper::Install
     *
     * \param n1 the name of the attribute to set
     * \param v1 the value of the attribute to set
     */
    void SetDeviceAttribute(std::string n1, const AttributeValue& v1);
    /**
     * This method creates an ns3::ATPBridgeNetDevice with the attributes
     * configured by ATPBridgeHelper::SetDeviceAttribute, adds the device
     * to the node, and attaches the given NetDevices as ports of the
     * bridge.
     *
     * \param node The node to install the device in
     * \param c Container of NetDevices to add as bridge ports
     * \returns A container holding the added net device.
     */
    NetDeviceContainer Install(Ptr<Node> node, NetDeviceContainer c);
    /**
     * This method creates an ns3::ATPBridgeNetDevice with the attributes
     * configured by ATPBridgeHelper::SetDeviceAttribute, adds the device
     * to the node, and attaches the given NetDevices as ports of the
     * bridge.
     *
     * \param nodeName The name of the node to install the device in
     * \param c Container of NetDevices to add as bridge ports
     * \returns A container holding the added net device.
     */
    NetDeviceContainer Install(std::string nodeName, NetDeviceContainer c);

  private:
    ObjectFactory m_deviceFactory; //!< Object factory
};

} // namespace ns3

#endif /* ATP_BRIDGE_HELPER_H */
