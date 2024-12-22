/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Gustavo Carneiro  <gjc@inescporto.pt>
 */

#include "atp-bridge-channel.h"

#include "ns3/log.h"

/**
 * \file
 * \ingroup bridge
 * ns3::ATPBridgeChannel implementation.
 */

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPBridgeChannel");

NS_OBJECT_ENSURE_REGISTERED(ATPBridgeChannel);

TypeId
ATPBridgeChannel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ATPBridgeChannel")
                            .SetParent<Channel>()
                            .SetGroupName("Bridge")
                            .AddConstructor<ATPBridgeChannel>();
    return tid;
}

ATPBridgeChannel::ATPBridgeChannel()
    : Channel()
{
    NS_LOG_FUNCTION_NOARGS();
}

ATPBridgeChannel::~ATPBridgeChannel()
{
    NS_LOG_FUNCTION_NOARGS();

    for (auto iter = m_bridgedChannels.begin(); iter != m_bridgedChannels.end(); iter++)
    {
        *iter = nullptr;
    }
    m_bridgedChannels.clear();
}

void
ATPBridgeChannel::AddChannel(Ptr<Channel> bridgedChannel)
{
    m_bridgedChannels.push_back(bridgedChannel);
}

std::size_t
ATPBridgeChannel::GetNDevices() const
{
    uint32_t ndevices = 0;
    for (auto iter = m_bridgedChannels.begin(); iter != m_bridgedChannels.end(); iter++)
    {
        ndevices += (*iter)->GetNDevices();
    }
    return ndevices;
}

Ptr<NetDevice>
ATPBridgeChannel::GetDevice(std::size_t i) const
{
    std::size_t ndevices = 0;
    for (auto iter = m_bridgedChannels.begin(); iter != m_bridgedChannels.end(); iter++)
    {
        if ((i - ndevices) < (*iter)->GetNDevices())
        {
            return (*iter)->GetDevice(i - ndevices);
        }
        ndevices += (*iter)->GetNDevices();
    }
    return nullptr;
}

} // namespace ns3
