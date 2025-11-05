/*
 * Copyright (c) 2010 Georgia Institute of Technology
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

#ifndef ATP_BULK_SEND_APPLICATION_H
#define ATP_BULK_SEND_APPLICATION_H

#include "atp-tag.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class Address;
class Socket;
class ATPTag;
class ATPSocket;

/**
 * \ingroup applications
 * \defgroup bulksend BulkSendApplication
 *
 * This traffic generator simply sends data
 * as fast as possible up to MaxBytes or until
 * the application is stopped (if MaxBytes is
 * zero). Once the lower layer send buffer is
 * filled, it waits until space is free to
 * send more data, essentially keeping a
 * constant flow of data. Only SOCK_STREAM
 * and SOCK_SEQPACKET sockets are supported.
 * For example, TCP sockets can be used, but
 * UDP sockets can not be used.
 */

/**
 * \ingroup bulksend
 *
 * \brief Send as much traffic as possible, trying to fill the bandwidth.
 *
 * This traffic generator simply sends data
 * as fast as possible up to MaxBytes or until
 * the application is stopped (if MaxBytes is
 * zero). Once the lower layer send buffer is
 * filled, it waits until space is free to
 * send more data, essentially keeping a
 * constant flow of data. Only SOCK_STREAM
 * and SOCK_SEQPACKET sockets are supported.
 * For example, TCP sockets can be used, but
 * UDP sockets can not be used.
 *
 * If the attribute "EnableSeqTsSizeHeader" is enabled, the application will
 * use some bytes of the payload to store an header with a sequence number,
 * a timestamp, and the size of the packet sent. Support for extracting
 * statistics from this header have been added to \c ns3::PacketSink
 * (enable its "EnableSeqTsSizeHeader" attribute), or users may extract
 * the header via trace sources.
 */
class ATPBulkSendApplication : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    ATPBulkSendApplication();

    ~ATPBulkSendApplication() override;

    /**
     * \brief Set the upper bound for the total number of bytes to send.
     *
     * Once this bound is reached, no more application bytes are sent. If the
     * application is stopped during the simulation and restarted, the
     * total number of bytes sent is not reset; however, the maxBytes
     * bound is still effective and the application will continue sending
     * up to maxBytes. The value zero for maxBytes means that
     * there is no upper bound; i.e. data is sent until the application
     * or simulation is stopped.
     *
     * \param maxBytes the upper bound of bytes to send
     */
    void SetMaxBytes(uint64_t maxBytes);

    /**
     * \brief Get the socket this application is attached to.
     * \return pointer to associated socket
     */
    Ptr<Socket> GetSocket() const;

    /**
     * \brief Setup the application with the given parameters.
     *
     * This function sets up the application with the given parameters.
     *
     * \param sinkAddress the sink address
     * \param socket the socket
     * \param maxBytes the upper bound of bytes to send
     * \param jobId the job ID
     */
    void Setup(Address sinkAddress, Ptr<Socket> socket, uint64_t maxBytes, uint8_t jobId);

    /**
     * \brief Set the socket for the application.
     *
     * This function sets the socket for the application.
     *
     * \param socket the socket to set
     */
    void SetSocket(Ptr<Socket> socket);

    void SetEnableATPTag(bool enableATPTag);
    bool GetEnableATPTag() const;

    void SetJobId(uint8_t jobId);
    uint8_t GetJobId() const;

    void SetFaninDegree0(uint8_t faninDegree0);
    uint8_t GetFaninDegree0() const;

    void SetFaninDegree1(uint8_t faninDegree1);
    uint8_t GetFaninDegree1() const;

    void SetBitmap0(uint32_t bitmap0);
    uint32_t GetBitmap0() const;

    void SetBitmap1(uint32_t bitmap1);
    uint32_t GetBitmap1() const;

    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);
    void DataSend(Ptr<Socket> socket, uint32_t);

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * \brief Send data until the L4 transmission buffer is full.
     * \param from From address
     * \param to To address
     */
    void SendData(const Address& from, const Address& to);

    Ptr<Socket> m_socket;                //!< Associated socket
    Address m_peer;                      //!< Peer address
    Address m_local;                     //!< Local address to bind to
    bool m_connected;                    //!< True if connected
    uint16_t m_sendSize;                 //!< Size of data to send each time
    uint64_t m_maxBytes;                 //!< Limit total number of bytes sent
    uint64_t m_totBytes;                 //!< Total bytes sent so far
    TypeId m_tid;                        //!< The type of protocol to use.
    Ptr<Packet> m_unsentPacket;          //!< Variable to cache unsent packet
    
    // ATPTag related attributes
    bool m_enableATPTag{false};
    uint8_t m_jobId{0};
    uint32_t m_seqNum{0};
    uint8_t m_faninDegree0{0};
    uint8_t m_faninDegree1{0};
    uint32_t m_bitmap0{0};
    uint32_t m_bitmap1{0};

    /// Traced Callback: sent packets
    TracedCallback<Ptr<const Packet>> m_txTrace;
};

} // namespace ns3

#endif /* ATP_BULK_SEND_APPLICATION_H */
