/*
 * Copyright (c) 2007 Emmanuelle Laprise
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Emmanuelle Laprise<emmanuelle.laprise@bluekazoo.ca>
 */

#ifndef ATP_CSMA_CHANNEL_H
#define ATP_CSMA_CHANNEL_H

#include "ns3/channel.h"
#include "ns3/data-rate.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"

namespace ns3
{

class Packet;

class ATPCsmaNetDevice;

/**
 * \ingroup csma
 * \brief ATPCsmaNetDevice Record
 *
 * Stores the information related to each net device that is
 * connected to the channel.
 */
class ATPCsmaDeviceRec
{
  public:
    Ptr<ATPCsmaNetDevice> devicePtr; //!< Pointer to the net device
    bool active;                  //!< Is net device enabled to TX/RX

    ATPCsmaDeviceRec();

    /**
     * \brief Constructor
     * Builds a record of the given NetDevice, its status is initialized to enabled.
     *
     * \param device the device to record
     */
    ATPCsmaDeviceRec(Ptr<ATPCsmaNetDevice> device);

    /**
     * Copy constructor
     * \param o the object to copy
     */
    ATPCsmaDeviceRec(const ATPCsmaDeviceRec& o);

    /**
     * \return If the net device pointed to by the devicePtr is active
     * and ready to RX/TX.
     */
    bool IsActive() const;
};

/**
 * Current state of the channel
 */
enum ATPWireState
{
    ATP_IDLE,         /**< Channel is IDLE, no packet is being transmitted */
    ATP_TRANSMITTING, /**< Channel is BUSY, a packet is being written by a net device */
    ATP_PROPAGATING   /**< Channel is BUSY, packet is propagating to all attached net devices */
};

/**
 * \ingroup csma
 * \brief ATP Csma Channel.
 *
 * This class represents a simple Csma channel that can be used
 * when many nodes are connected to one wire. It uses a single busy
 * flag to indicate if the channel is currently in use. It does not
 * take into account the distances between stations or the speed of
 * light to determine collisions.
 */
class ATPCsmaChannel : public Channel
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief Create a ATPCsmaChannel
     */
    ATPCsmaChannel();
    /**
     * \brief Destroy a ATPCsmaChannel
     */
    ~ATPCsmaChannel() override;

    // Delete copy constructor and assignment operator to avoid misuse
    ATPCsmaChannel(const ATPCsmaChannel&) = delete;
    ATPCsmaChannel& operator=(const ATPCsmaChannel&) = delete;

    /**
     * \brief Attach a given netdevice to this channel
     *
     * \param device Device pointer to the netdevice to attach to the channel
     * \return The assigned device number
     */
    int32_t Attach(Ptr<ATPCsmaNetDevice> device);

    /**
     * \brief Detach a given netdevice from this channel
     *
     * The net device is marked as inactive and it is not allowed to
     * receive or transmit packets
     *
     * \param device Device pointer to the netdevice to detach from the channel
     * \return True if the device is found and attached to the channel,
     * false if the device is not currently connected to the channel or
     * can't be found.
     */
    bool Detach(Ptr<ATPCsmaNetDevice> device);

    /**
     * \brief Detach a given netdevice from this channel
     *
     * The net device is marked as inactive and it is not allowed to
     * receive or transmit packets
     *
     * \param deviceId The deviceID assigned to the net device when it
     * was connected to the channel
     * \return True if the device is found and attached to the channel,
     * false if the device is not currently connected to the channel or
     * can't be found.
     */
    bool Detach(uint32_t deviceId);

    /**
     * \brief Reattach a previously detached net device to the channel
     *
     * The net device is marked as active. It is now allowed to receive
     * or transmit packets. The net device must have been previously
     * attached to the channel using the attach function.
     *
     * \param deviceId The device ID assigned to the net device when it
     * was connected to the channel
     * \return True if the device is found and is not attached to the
     * channel, false if the device is currently connected to the
     * channel or can't be found.
     */
    bool Reattach(uint32_t deviceId);

    /**
     * \brief Reattach a previously detached net device to the channel
     *
     * The net device is marked as active. It is now allowed to receive
     * or transmit packets. The net device must have been previously
     * attached to the channel using the attach function.
     *
     * \param device Device pointer to the netdevice to detach from the channel
     * \return True if the device is found and is not attached to the
     * channel, false if the device is currently connected to the
     * channel or can't be found.
     */
    bool Reattach(Ptr<ATPCsmaNetDevice> device);

    /**
     * \brief Start transmitting a packet over the channel
     *
     * If the srcId belongs to a net device that is connected to the
     * channel, packet transmission begins, and the channel becomes busy
     * until the packet has completely reached all destinations.
     *
     * \param p A reference to the packet that will be transmitted over
     * the channel
     * \param srcId The device Id of the net device that wants to
     * transmit on the channel.
     * \return True if the channel is not busy and the transmitting net
     * device is currently active.
     */
    bool TransmitStart(Ptr<const Packet> p, uint32_t srcId);

    /**
     * \brief Indicates that the net device has finished transmitting
     * the packet over the channel
     *
     * The channel will stay busy until the packet has completely
     * propagated to all net devices attached to the channel. The
     * TransmitEnd function schedules the PropagationCompleteEvent which
     * will free the channel for further transmissions. Stores the
     * packet p as the m_currentPkt, the packet being currently
     * transmitting.
     *
     * \return Returns true unless the source was detached before it
     * completed its transmission.
     */
    bool TransmitEnd();

    /**
     * \brief Indicates that the channel has finished propagating the
     * current packet. The channel is released and becomes free.
     *
     * Calls the receive function of every active net device that is
     * attached to the channel.
     */
    void PropagationCompleteEvent();

    /**
     * \return Returns the device number assigned to a net device by the
     * channel
     *
     * \param device Device pointer to the netdevice for which the device
     * number is needed
     */
    int32_t GetDeviceNum(Ptr<ATPCsmaNetDevice> device);

    /**
     * \return Returns the state of the channel (IDLE -- free,
     * TRANSMITTING -- busy, PROPAGATING - busy )
     */
    ATPWireState GetState();

    /**
     * \brief Indicates if the channel is busy. The channel will only
     * accept new packets for transmission if it is not busy.
     *
     * \return Returns true if the channel is busy and false if it is
     * free.
     */
    bool IsBusy();

    /**
     * \brief Indicates if a net device is currently attached or
     * detached from the channel.
     *
     * \param deviceId The ID that was assigned to the net device when
     * it was attached to the channel.
     * \return Returns true if the net device is attached to the
     * channel, false otherwise.
     */
    bool IsActive(uint32_t deviceId);

    /**
     * \return Returns the number of net devices that are currently
     * attached to the channel.
     */
    uint32_t GetNumActDevices();

    /**
     * \return Returns the total number of devices including devices
     * that have been detached from the channel.
     */
    std::size_t GetNDevices() const override;

    /**
     * \return Get a NetDevice pointer to a connected network device.
     *
     * \param i The index of the net device.
     * \return Returns the pointer to the net device that is associated
     * with deviceId i.
     */
    Ptr<NetDevice> GetDevice(std::size_t i) const override;

    /**
     * \return Get a ATPCsmaNetDevice pointer to a connected network device.
     *
     * \param i The deviceId of the net device for which we want the
     * pointer.
     * \return Returns the pointer to the net device that is associated
     * with deviceId i.
     */
    Ptr<ATPCsmaNetDevice> GetATPCsmaDevice(std::size_t i) const;

    /**
     * Get the assigned data rate of the channel
     *
     * \return Returns the DataRate to be used by device transmitters.
     * with deviceId i.
     */
    DataRate GetDataRate();

    /**
     * Get the assigned speed-of-light delay of the channel
     *
     * \return Returns the delay used by the channel.
     */
    Time GetDelay();

  private:
    /**
     * The assigned data rate of the channel
     */
    DataRate m_bps;

    /**
     * The assigned speed-of-light delay of the channel
     */
    Time m_delay;

    /**
     * List of the net devices that have been or are currently connected
     * to the channel.
     *
     * Devices are nor removed from this list, they are marked as
     * inactive. Otherwise the assigned device IDs will not refer to the
     * correct NetDevice. The DeviceIds are used so that it is possible
     * to have a number to refer to an entry in the list so that the
     * whole list does not have to be searched when making sure that a
     * source is attached to a channel when it is transmitting data.
     */
    std::vector<ATPCsmaDeviceRec> m_deviceList;

    /**
     * The Packet that is currently being transmitted on the channel (or last
     * packet to have been transmitted on the channel if the channel is
     * free.)
     */
    Ptr<const Packet> m_currentPkt;

    /**
     * Device Id of the source that is currently transmitting on the
     * channel. Or last source to have transmitted a packet on the
     * channel, if the channel is currently not busy.
     */
    uint32_t m_currentSrc;

    /**
     * Current state of the channel
     */
    ATPWireState m_state;
};

} // namespace ns3

#endif /* ATP_CSMA_CHANNEL_H */
