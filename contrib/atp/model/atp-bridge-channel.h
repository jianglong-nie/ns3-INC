/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Gustavo Carneiro  <gjc@inescporto.pt>
 */
#ifndef ATP_BRIDGE_CHANNEL_H
#define ATP_BRIDGE_CHANNEL_H

#include "ns3/channel.h"
#include "ns3/net-device.h"

#include <vector>

/**
 * \file
 * \ingroup bridge
 * ns3::BridgeChannel declaration.
 */

namespace ns3
{

/**
 * \ingroup bridge
 *
 * \brief Virtual channel implementation for bridges (BridgeNetDevice).
 *
 * Just like BridgeNetDevice aggregates multiple NetDevices,
 * BridgeChannel aggregates multiple channels and make them appear as
 * a single channel to upper layers.
 */
class ATPBridgeChannel : public Channel
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    ATPBridgeChannel();
    ~ATPBridgeChannel() override;

    // Delete copy constructor and assignment operator to avoid misuse
    ATPBridgeChannel(const ATPBridgeChannel&) = delete;
    ATPBridgeChannel& operator=(const ATPBridgeChannel&) = delete;

    /**
     * Adds a channel to the bridged pool
     * \param bridgedChannel  the channel to add to the pool
     */
    void AddChannel(Ptr<Channel> bridgedChannel);

    // virtual methods implementation, from Channel
    std::size_t GetNDevices() const override;
    Ptr<NetDevice> GetDevice(std::size_t i) const override;

  private:
    std::vector<Ptr<Channel>> m_bridgedChannels; //!< pool of bridged channels
};

} // namespace ns3

#endif /* ATP_BRIDGE_CHANNEL_H */
