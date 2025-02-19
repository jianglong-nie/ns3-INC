/*
 * Copyright (c) 2007 Emmanuelle Laprise
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Emmanuelle Laprise <emmanuelle.laprise@bluekazoo.ca>
 */

#include "atp-csma-channel.h"

#include "atp-csma-net-device.h"

#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ATPCsmaChannel");

NS_OBJECT_ENSURE_REGISTERED(ATPCsmaChannel);

TypeId
ATPCsmaChannel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ATPCsmaChannel")
            .SetParent<Channel>()
            .SetGroupName("Csma")
            .AddConstructor<ATPCsmaChannel>()
            .AddAttribute(
                "DataRate",
                "The transmission data rate to be provided to devices connected to the channel",
                DataRateValue(DataRate(0xffffffff)),
                MakeDataRateAccessor(&ATPCsmaChannel::m_bps),
                MakeDataRateChecker())
            .AddAttribute("Delay",
                          "Transmission delay through the channel",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&ATPCsmaChannel::m_delay),
                          MakeTimeChecker());
    return tid;
}

ATPCsmaChannel::ATPCsmaChannel()
    : Channel()
{
    NS_LOG_FUNCTION_NOARGS();
    m_state = ATP_IDLE;
    m_deviceList.clear();
}

ATPCsmaChannel::~ATPCsmaChannel()
{
    NS_LOG_FUNCTION(this);
    m_deviceList.clear();
}

int32_t
ATPCsmaChannel::Attach(Ptr<ATPCsmaNetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    NS_ASSERT(device);

    ATPCsmaDeviceRec rec(device);

    m_deviceList.push_back(rec);
    return (m_deviceList.size() - 1);
}

bool
ATPCsmaChannel::Reattach(Ptr<ATPCsmaNetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    NS_ASSERT(device);

    for (auto it = m_deviceList.begin(); it < m_deviceList.end(); it++)
    {
        if (it->devicePtr == device)
        {
            if (!it->active)
            {
                it->active = true;
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    return false;
}

bool
ATPCsmaChannel::Reattach(uint32_t deviceId)
{
    NS_LOG_FUNCTION(this << deviceId);

    if (deviceId < m_deviceList.size())
    {
        return false;
    }

    if (m_deviceList[deviceId].active)
    {
        return false;
    }
    else
    {
        m_deviceList[deviceId].active = true;
        return true;
    }
}

bool
ATPCsmaChannel::Detach(uint32_t deviceId)
{
    NS_LOG_FUNCTION(this << deviceId);

    if (deviceId < m_deviceList.size())
    {
        if (!m_deviceList[deviceId].active)
        {
            NS_LOG_WARN("ATPCsmaChannel::Detach(): Device is already detached (" << deviceId << ")");
            return false;
        }

        m_deviceList[deviceId].active = false;

        if ((m_state == ATP_TRANSMITTING) && (m_currentSrc == deviceId))
        {
            NS_LOG_WARN("ATPCsmaChannel::Detach(): Device is currently"
                        << "ATP_TRANSMITTING (" << deviceId << ")");
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool
ATPCsmaChannel::Detach(Ptr<ATPCsmaNetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    NS_ASSERT(device);

    for (auto it = m_deviceList.begin(); it < m_deviceList.end(); it++)
    {
        if ((it->devicePtr == device) && (it->active))
        {
            it->active = false;
            return true;
        }
    }
    return false;
}

bool
ATPCsmaChannel::TransmitStart(Ptr<const Packet> p, uint32_t srcId)
{
    NS_LOG_FUNCTION(this << p << srcId);
    NS_LOG_INFO("UID is " << p->GetUid() << ")");

    if (m_state != ATP_IDLE)
    {
        NS_LOG_WARN("ATPCsmaChannel::TransmitStart(): State is not ATP_IDLE");
        return false;
    }

    if (!IsActive(srcId))
    {
        NS_LOG_ERROR(
            "ATPCsmaChannel::TransmitStart(): Seclected source is not currently attached to network");
        return false;
    }

    NS_LOG_LOGIC("switch to ATP_TRANSMITTING");
    m_currentPkt = p;
    m_currentSrc = srcId;
    m_state = ATP_TRANSMITTING;
    return true;
}

bool
ATPCsmaChannel::IsActive(uint32_t deviceId)
{
    return m_deviceList[deviceId].active;
}

bool
ATPCsmaChannel::TransmitEnd()
{
    NS_LOG_FUNCTION(this << m_currentPkt << m_currentSrc);
    NS_LOG_INFO("UID is " << m_currentPkt->GetUid() << ")");

    NS_ASSERT(m_state == ATP_TRANSMITTING);
    m_state = ATP_PROPAGATING;

    bool retVal = true;

    if (!IsActive(m_currentSrc))
    {
        NS_LOG_ERROR("ATPCsmaChannel::TransmitEnd(): Seclected source was detached before the end of "
                     "the transmission");
        retVal = false;
    }

    NS_LOG_LOGIC("Schedule event in " << m_delay.As(Time::S));

    NS_LOG_LOGIC("Receive");

    for (auto it = m_deviceList.begin(); it < m_deviceList.end(); it++)
    {
        if (it->IsActive() && it->devicePtr != m_deviceList[m_currentSrc].devicePtr)
        {
            // schedule reception events
            Simulator::ScheduleWithContext(it->devicePtr->GetNode()->GetId(),
                                           m_delay,
                                           &ATPCsmaNetDevice::Receive,
                                           it->devicePtr,
                                           m_currentPkt,
                                           m_deviceList[m_currentSrc].devicePtr);
        }
    }

    // also schedule for the tx side to go back to ATP_IDLE
    Simulator::Schedule(m_delay, &ATPCsmaChannel::PropagationCompleteEvent, this);
    return retVal;
}

void
ATPCsmaChannel::PropagationCompleteEvent()
{
    NS_LOG_FUNCTION(this << m_currentPkt);
    NS_LOG_INFO("UID is " << m_currentPkt->GetUid() << ")");

    NS_ASSERT(m_state == ATP_PROPAGATING);
    m_state = ATP_IDLE;
}

uint32_t
ATPCsmaChannel::GetNumActDevices()
{
    int numActDevices = 0;
    for (auto it = m_deviceList.begin(); it < m_deviceList.end(); it++)
    {
        if (it->active)
        {
            numActDevices++;
        }
    }
    return numActDevices;
}

std::size_t
ATPCsmaChannel::GetNDevices() const
{
    return m_deviceList.size();
}

Ptr<ATPCsmaNetDevice>
ATPCsmaChannel::GetATPCsmaDevice(std::size_t i) const
{
    return m_deviceList[i].devicePtr;
}

int32_t
ATPCsmaChannel::GetDeviceNum(Ptr<ATPCsmaNetDevice> device)
{
    int i = 0;
    for (auto it = m_deviceList.begin(); it < m_deviceList.end(); it++)
    {
        if (it->devicePtr == device)
        {
            if (it->active)
            {
                return i;
            }
            else
            {
                return -2;
            }
        }
        i++;
    }
    return -1;
}

bool
ATPCsmaChannel::IsBusy()
{
    return m_state != ATP_IDLE;
}

DataRate
ATPCsmaChannel::GetDataRate()
{
    return m_bps;
}

Time
ATPCsmaChannel::GetDelay()
{
    return m_delay;
}

ATPWireState
ATPCsmaChannel::GetState()
{
    return m_state;
}

Ptr<NetDevice>
ATPCsmaChannel::GetDevice(std::size_t i) const
{
    return GetATPCsmaDevice(i);
}

ATPCsmaDeviceRec::ATPCsmaDeviceRec()
{
    active = false;
}

ATPCsmaDeviceRec::ATPCsmaDeviceRec(Ptr<ATPCsmaNetDevice> device)
{
    devicePtr = device;
    active = true;
}

ATPCsmaDeviceRec::ATPCsmaDeviceRec(const ATPCsmaDeviceRec& deviceRec)
{
    devicePtr = deviceRec.devicePtr;
    active = deviceRec.active;
}

bool
ATPCsmaDeviceRec::IsActive() const
{
    return active;
}

} // namespace ns3
