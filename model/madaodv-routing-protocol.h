/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on
 *      NS-2 AODV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 *
 *      AODV-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AODV-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */
#ifndef MADAODVROUTINGPROTOCOL_H
#define MADAODVROUTINGPROTOCOL_H

#include "madaodv-rtable.h"
#include "madaodv-rqueue.h"
#include "madaodv-packet.h"
#include "madaodv-neighbor.h"
#include "madaodv-dpd.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/ipv6-interface.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/net-device.h"
#include <map>

namespace ns3 {

class WifiMacQueueItem;
enum WifiMacDropReason : uint8_t;  // opaque enum declaration

namespace madaodv {
/**
 * \ingroup aodv
 *
 * \brief AODV routing protocol
 */
class RoutingProtocol : public Ipv6RoutingProtocol
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static const uint32_t MADAODV_PORT;
  static const char* BROADCAST_ADDR;

  /// constructor
  RoutingProtocol ();
  virtual ~RoutingProtocol ();
  virtual void DoDispose ();

  // Inherited from Ipv6RoutingProtocol
  Ptr<Ipv6Route> RouteOutput (Ptr<Packet> p, const Ipv6Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv6Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv6InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv6InterfaceAddress address);
  virtual void NotifyAddRoute	(Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t 	interface, Ipv6Address prefixToUse = Ipv6Address::GetZero());
  virtual void NotifyRemoveRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t 	interface, Ipv6Address prefixToUse = Ipv6Address::GetZero());
  virtual void SetIpv6 (Ptr<Ipv6> ipv6);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;


  /**
   * Add an ipv6 interface for a device with ipv6 address corresponding to device mac address
   * \param dev the device
   * \returns whether operation was successful
   */
//  bool AddDevInterface(Ptr<NetDevice> dev);

  // Handle protocol parameters
  /**
   * Get maximum queue time
   * \returns the maximum queue time
   */
  Time GetMaxQueueTime () const
  {
    return m_maxQueueTime;
  }
  /**
   * Set the maximum queue time
   * \param t the maximum queue time
   */
  void SetMaxQueueTime (Time t);
  /**
   * Get the maximum queue length
   * \returns the maximum queue length
   */
  uint32_t GetMaxQueueLen () const
  {
    return m_maxQueueLen;
  }
  /**
   * Set the maximum queue length
   * \param len the maximum queue length
   */
  void SetMaxQueueLen (uint32_t len);
  /**
   * Get destination only flag
   * \returns the destination only flag
   */
  bool GetDestinationOnlyFlag () const
  {
    return m_destinationOnly;
  }
  /**
   * Set destination only flag
   * \param f the destination only flag
   */
  void SetDestinationOnlyFlag (bool f)
  {
    m_destinationOnly = f;
  }
  /**
   * Get gratuitous reply flag
   * \returns the gratuitous reply flag
   */
  bool GetGratuitousReplyFlag () const
  {
    return m_gratuitousReply;
  }
  /**
   * Set gratuitous reply flag
   * \param f the gratuitous reply flag
   */
  void SetGratuitousReplyFlag (bool f)
  {
    m_gratuitousReply = f;
  }
  /**
   * Set hello enable
   * \param f the hello enable flag
   */
  void SetHelloEnable (bool f)
  {
    m_enableHello = f;
  }
  /**
   * Get hello enable flag
   * \returns the enable hello flag
   */
  bool GetHelloEnable () const
  {
    return m_enableHello;
  }
  /**
   * Set broadcast enable flag
   * \param f enable broadcast flag
   */
  void SetBroadcastEnable (bool f)
  {
    m_enableBroadcast = f;
  }
  /**
   * Get broadcast enable flag
   * \returns the broadcast enable flag
   */
  bool GetBroadcastEnable () const
  {
    return m_enableBroadcast;
  }

  /**
   * Set whether we are an access point
   * \param f whether we are access point
   */
  void SetAmAccessPoint (bool f)
  {
    m_amAccessPoint = f;
  }
  /**
   * Get whether we are an access point
   * \returns whether we are access point
   */
  bool GetAmAccessPoint () const
  {
    return m_amAccessPoint;
  }


  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model.  Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this model
   */
  int64_t AssignStreams (int64_t stream);

protected:
  virtual void DoInitialize (void);
private:
  /**
   * Notify that an MPDU was dropped.
   *
   * \param reason the reason why the MPDU was dropped
   * \param mpdu the dropped MPDU
   */
  void NotifyTxError (WifiMacDropReason reason, Ptr<const WifiMacQueueItem> mpdu);

  // Protocol parameters.
  uint32_t m_rreqRetries;             ///< Maximum number of retransmissions of RREQ with TTL = NetDiameter to discover a route
  uint16_t m_ttlStart;                ///< Initial TTL value for RREQ.
  uint16_t m_ttlIncrement;            ///< TTL increment for each attempt using the expanding ring search for RREQ dissemination.
  uint16_t m_ttlThreshold;            ///< Maximum TTL value for expanding ring search, TTL = NetDiameter is used beyond this value.
  uint16_t m_timeoutBuffer;           ///< Provide a buffer for the timeout.
  uint16_t m_rreqRateLimit;           ///< Maximum number of RREQ per second.
  uint16_t m_rerrRateLimit;           ///< Maximum number of REER per second.
  Time m_activeRouteTimeout;          ///< Period of time during which the route is considered to be valid.
  uint32_t m_netDiameter;             ///< Net diameter measures the maximum possible number of hops between two nodes in the network
  /**
   *  NodeTraversalTime is a conservative estimate of the average one hop traversal time for packets
   *  and should include queuing delays, interrupt processing times and transfer times.
   */
  Time m_nodeTraversalTime;
  Time m_netTraversalTime;             ///< Estimate of the average net traversal time.
  Time m_pathDiscoveryTime;            ///< Estimate of maximum time needed to find route in network.
  Time m_myRouteTimeout;               ///< Value of lifetime field in RREP generating by this node.
  /**
   * Every HelloInterval the node checks whether it has sent a broadcast  within the last HelloInterval.
   * If it has not, it MAY broadcast a  Hello message
   */
  Time m_helloInterval;
  uint32_t m_allowedHelloLoss;         ///< Number of hello messages which may be loss for valid link
  /**
   * DeletePeriod is intended to provide an upper bound on the time for which an upstream node A
   * can have a neighbor B as an active next hop for destination D, while B has invalidated the route to D.
   */
  Time m_deletePeriod;
  Time m_nextHopWait;                  ///< Period of our waiting for the neighbour's RREP_ACK
  Time m_blackListTimeout;             ///< Time for which the node is put into the blacklist
  uint32_t m_maxQueueLen;              ///< The maximum number of packets that we allow a routing protocol to buffer.
  Time m_maxQueueTime;                 ///< The maximum period of time that a routing protocol is allowed to buffer a packet for.
  bool m_destinationOnly;              ///< Indicates only the destination may respond to this RREQ.
  bool m_gratuitousReply;              ///< Indicates whether a gratuitous RREP should be unicast to the node originated route discovery.
  bool m_enableHello;                  ///< Indicates whether a hello messages enable
  bool m_enableBroadcast;              ///< Indicates whether a a broadcast data packets forwarding enable
  //\}

  /// IP protocol
  Ptr<Ipv6> m_ipv6;
  /// Raw unicast socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv6InterfaceAddress > m_socketAddresses;
  /// Raw subnet directed broadcast socket per each IP interface, map socket -> iface address (IP + mask)
 // std::map< Ptr<Socket>, Ipv6InterfaceAddress > m_socketSubnetBroadcastAddresses;
  /// Loopback device used to defer RREQ until packet will be fully formed
  Ptr<NetDevice> m_lo;

  /// Routing table
  RoutingTable m_routingTable;
  /// A "drop-front" queue used by the routing layer to buffer packets to which it does not have a route.
  RequestQueue m_queue;
  /// Broadcast ID
  uint32_t m_requestId;
  /// Request sequence number
  uint32_t m_seqNo;
  /// Handle duplicated RREQ
  IdCache m_rreqIdCache;
  /// Handle duplicated broadcast/multicast packets
  DuplicatePacketDetection m_dpd;
  /// Handle neighbors
  Neighbors m_nb;
  /// Number of RREQs used for RREQ rate control
  uint16_t m_rreqCount;
  /// Number of RERRs used for RERR rate control
  uint16_t m_rerrCount;

private:
  /// Start protocol operation
  void Start ();

  /**
   * Convert ipv6 to mac address
   *
   * \param ipv6Addr the ipv6 address that needs to be converted
   * \returns the converted mac address
   */ 
  Mac48Address Ipv6ToMac (Ipv6Address ipv6Addr);

  /**
   * Convert mac to ipv6 address
   *
   * \param macAddr the mac address that needs to be converted
   * \returns the converted ipv6 address
   */ 
  Ipv6Address MacToIpv6 (Mac48Address macAddr);

  /**
   * Convert mac to ipv6 address
   *
   * \param addr the ipv6 address
   * \returns whether addr is an internet address (not part of our address range)
   */ 
   bool OnInternet(Ipv6Address addr);


  /**
   * Queue packet and send route request
   *
   * \param p the packet to route
   * \param header the IP header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   */ 
  void DeferredRouteOutput (Ptr<const Packet> p, const Ipv6Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);

  /**
   * If route exists and is valid, forward packet.
   *
   * \param p the packet to route
   * \param header the IP header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   * \returns true if forwarded
   */ 
  bool Forwarding (Ptr<const Packet> p, const Ipv6Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /**
   * Repeated attempts by a source node at route discovery for a single destination
   * use the expanding ring search technique.
   * \param dst the destination IP address
   */
  void ScheduleRreqRetry (Ipv6Address dst);
  /**
   * Set lifetime field in routing table entry to the maximum of existing lifetime and lt, if the entry exists
   * \param addr - destination address
   * \param lt - proposed time for lifetime field in routing table entry for destination with address addr.
   * \return true if route to destination address addr exist
   */
  bool UpdateRouteLifeTime (Ipv6Address addr, Time lt);
  /**
   * Update neighbor record.
   * \param receiver is supposed to be my interface
   * \param sender is supposed to be IP address of my neighbor.
   */
  void UpdateRouteToNeighbor (Ipv6Address sender, Ipv6Address receiver);
  /**
   * Test whether the provided address is assigned to an interface on this node
   * \param src the source IP address
   * \returns true if the IP address is the node's IP address
   */
  bool IsMyOwnAddress (Ipv6Address src);
  /**
   * Find unicast socket with local interface address iface
   *
   * \param iface the interface
   * \returns the socket associated with the interface
   */
  Ptr<Socket> FindSocketWithInterfaceAddress (Ipv6InterfaceAddress iface) const;
  /**
   * Find subnet directed broadcast socket with local interface address iface
   *
   * \param iface the interface
   * \returns the socket associated with the interface
   */
 // Ptr<Socket> FindSubnetBroadcastSocketWithInterfaceAddress (Ipv6InterfaceAddress iface) const;
  /**
   * Process hello message
   * 
   * \param rrepHeader RREP message header
   * \param receiverIfaceAddr receiver interface IP address
   */
  void ProcessHello (RrepHeader const & rrepHeader, Ipv6Address receiverIfaceAddr);
  /**
   * Create loopback route for given header
   *
   * \param header the IP header
   * \param oif the output interface net device
   * \returns the route
   */
  Ptr<Ipv6Route> LoopbackRoute (const Ipv6Header & header, Ptr<NetDevice> oif) const;

  ///\name Receive control packets
  //\{
  /**
   * Receive and process control packet
   * \param socket input socket
   */
  void RecvAodv (Ptr<Socket> socket);
  /**
   * Receive RREQ
   * \param p packet
   * \param receiver receiver address
   * \param src sender address
   */
  void RecvRequest (Ptr<Packet> p, Ipv6Address receiver, Ipv6Address src);
  /**
   * Receive RREP
   * \param p packet
   * \param my destination address
   * \param src sender address
   */
  void RecvReply (Ptr<Packet> p, Ipv6Address my, Ipv6Address src);
  /**
   * Receive RREP_ACK
   * \param neighbor neighbor address
   */
  void RecvReplyAck (Ipv6Address neighbor);
  /**
   * Receive RERR
   * \param p packet
   * \param src sender address
   */
  /// Receive  from node with address src
  void RecvError (Ptr<Packet> p, Ipv6Address src);
  //\}

  ///\name Send
  //\{
  /** Forward packets waiting for an accesspoint from route request queue to an access point
   * \param route route to access point
   */
  void SendApPacketsFromQueue (Ptr<Ipv6Route> route);

  /** Forward packet from route request queue
   * \param dst destination address
   * \param route route to use
   */
  void SendPacketFromQueue (Ipv6Address dst, Ptr<Ipv6Route> route);
  /// Send hello
  void SendHello ();
  /** Send RREQ
   * \param dst destination address
   */
  void SendRequest (Ipv6Address dst);
  /** Send RREP
   * \param rreqHeader route request header
   * \param toOrigin routing table entry to originator
   */
  void SendReply (RreqHeader const & rreqHeader, RoutingTableEntry const & toOrigin);
  /** Send RREP by intermediate node
   * \param toDst routing table entry to destination
   * \param toOrigin routing table entry to originator
   * \param gratRep indicates whether a gratuitous RREP should be unicast to destination
   */
  void SendReplyByIntermediateNode (RoutingTableEntry & toDst, RoutingTableEntry & toOrigin, bool gratRep);
  /** Send RREP_ACK
   * \param neighbor neighbor address
   */
  void SendReplyAck (Ipv6Address neighbor);
  /** Initiate RERR
   * \param nextHop next hop address
   */
  void SendRerrWhenBreaksLinkToNextHop (Ipv6Address nextHop);
  /** Forward RERR
   * \param packet packet
   * \param precursors list of addresses of the visited nodes
   */
  void SendRerrMessage (Ptr<Packet> packet,  std::vector<Ipv6Address> precursors);
  /**
   * Send RERR message when no route to forward input packet. Unicast if there is reverse route to originating node, broadcast otherwise.
   * \param dst - destination node IP address
   * \param dstSeqNo - destination node sequence number
   * \param origin - originating node IP address
   */
  void SendRerrWhenNoRouteToForward (Ipv6Address dst, uint32_t dstSeqNo, Ipv6Address origin);
  /// @}

  /**
   * Send packet to destination scoket
   * \param socket - destination node socket
   * \param packet - packet to send
   * \param destination - destination node IP address
   */
  void SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv6Address destination);

  /// Hello timer
  Timer m_htimer;
  /// Schedule next send of hello message
  void HelloTimerExpire ();
  /// RREQ rate limit timer
  Timer m_rreqRateLimitTimer;
  /// Reset RREQ count and schedule RREQ rate limit timer with delay 1 sec.
  void RreqRateLimitTimerExpire ();
  /// RERR rate limit timer
  Timer m_rerrRateLimitTimer;
  /// Reset RERR count and schedule RERR rate limit timer with delay 1 sec.
  void RerrRateLimitTimerExpire ();
  /// Map IP address + RREQ timer.

  std::map<Ipv6Address, Timer> m_addressReqTimer;
  /**
   * Handle route discovery process
   * \param dst the destination IP address
   */

   Timer m_associatedTimer;
  //Check whether any of the devices are associated with an access point
  void CheckAssociated (void);
  
  void RouteRequestTimerExpire (Ipv6Address dst);
  /**
   * Mark link to neighbor node as unidirectional for blacklistTimeout
   *
   * \param neighbor the IP address of the neightbor node
   * \param blacklistTimeout the black list timeout time
   */
  void AckTimerExpire (Ipv6Address neighbor, Time blacklistTimeout);

  /**
   * Determine whether address in range 100:0:0:0:0:*:*:* (100::/48)
   *
   * \param addr the Ipv6Address to test
   * \return true if addr is in range, false otherwise
   */
  bool InRange (Ipv6Address addr);

  /**
   * Return which index of Ipv6Interface corresponds to addr
   *
   * \param addr the Ipv6Address 
   * \return index or -1 if not found
   */
  int8_t GetIndexForAddress (uint8_t i, Ipv6Address addr);


  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_uniformRandomVariable;
  /// Keep track of the last bcast time
  Time m_lastBcastTime;

  //whether we are an access point
  bool m_amAccessPoint;

  //fisrt uint8_t represents interface index, second represents address index
  std::map<uint8_t, uint8_t> m_addresses;
};

} //namespace aodv
} //namespace ns3

#endif /* MADAODVROUTINGPROTOCOL_H */
