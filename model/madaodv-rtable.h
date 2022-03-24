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
#ifndef MADAODV_RTABLE_H
#define MADAODV_RTABLE_H

#include <stdint.h>
#include <cassert>
#include <map>
#include <sys/types.h>
#include "ns3/ipv6.h"
#include "ns3/ipv6-route.h"
#include "ns3/timer.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"

namespace ns3 {
namespace madaodv {

/**
 * \ingroup madaodv
 * \brief Route record states
 */
enum RouteFlags
{
  VALID = 0,          //!< VALID
  INVALID = 1,        //!< INVALID
  IN_SEARCH = 2,      //!< IN_SEARCH
};

/**
 * \ingroup madaodv
 * \brief Routing table entry
 */
class RoutingTableEntry
{
public:
  /**
   * constructor
   *
   * \param dev the device
   * \param dst the destination IP address
   * \param vSeqNo verify sequence number flag
   * \param seqNo the sequence number
   * \param iface the interface
   * \param hops the number of hops
   * \param nextHop the IP address of the next hop
   * \param lifetime the lifetime of the entry
   */
  RoutingTableEntry (Ptr<NetDevice> dev = 0,Ipv6Address dst = Ipv6Address (), bool vSeqNo = false, uint32_t seqNo = 0,
                     Ipv6InterfaceAddress iface = Ipv6InterfaceAddress (), uint16_t  hops = 0,
                     Ipv6Address nextHop = Ipv6Address (), Time lifetime = Simulator::Now ());

  ~RoutingTableEntry ();

  ///\name Precursors management
  //\{
  /**
   * Insert precursor in precursor list if it doesn't yet exist in the list
   * \param id precursor address
   * \return true on success
   */
  bool InsertPrecursor (Ipv6Address id);
  /**
   * Lookup precursor by address
   * \param id precursor address
   * \return true on success
   */
  bool LookupPrecursor (Ipv6Address id);
  /**
   * \brief Delete precursor
   * \param id precursor address
   * \return true on success
   */
  bool DeletePrecursor (Ipv6Address id);
  /// Delete all precursors
  void DeleteAllPrecursors ();
  /**
   * Check that precursor list is empty
   * \return true if precursor list is empty
   */
  bool IsPrecursorListEmpty () const;
  /**
   * Inserts precursors in output parameter prec if they do not yet exist in vector
   * \param prec vector of precursor addresses
   */
  void GetPrecursors (std::vector<Ipv6Address> & prec) const;
  //\}

  /**
   * Mark entry as "down" (i.e. disable it)
   * \param badLinkLifetime duration to keep entry marked as invalid
   */
  void Invalidate (Time badLinkLifetime);

  // Fields
  /**
   * Get destination address function
   * \returns the IPv6 destination address
   */
  Ipv6Address GetDestination () const
  {
    return m_ipv6Route->GetDestination ();
  }
  /**
   * Get route function
   * \returns The IPv6 route
   */
  Ptr<Ipv6Route> GetRoute () const
  {
    return m_ipv6Route;
  }
  /**
   * Set route function
   * \param r the IPv6 route
   */
  void SetRoute (Ptr<Ipv6Route> r)
  {
    m_ipv6Route = r;
  }
  /**
   * Set next hop address
   * \param nextHop the next hop IPv6 address
   */
  void SetNextHop (Ipv6Address nextHop)
  {
    m_ipv6Route->SetGateway (nextHop);
  }
  /**
   * Get next hop address
   * \returns the next hop address
   */
  Ipv6Address GetNextHop () const
  {
    return m_ipv6Route->GetGateway ();
  }
  /**
   * Set output device
   * \param dev The output device
   */
  void SetOutputDevice (Ptr<NetDevice> dev)
  {
    m_ipv6Route->SetOutputDevice (dev);
  }
  /**
   * Get output device
   * \returns the output device
   */
  Ptr<NetDevice> GetOutputDevice () const
  {
    return m_ipv6Route->GetOutputDevice ();
  }
  /**
   * Get the Ipv6InterfaceAddress
   * \returns the Ipv6InterfaceAddress
   */
  Ipv6InterfaceAddress GetInterface () const
  {
    return m_iface;
  }
  /**
   * Set the Ipv6InterfaceAddress
   * \param iface The Ipv6InterfaceAddress
   */
  void SetInterface (Ipv6InterfaceAddress iface)
  {
    m_iface = iface;
  }
  /**
   * Set the valid sequence number
   * \param s the sequence number
   */
  void SetValidSeqNo (bool s)
  {
    m_validSeqNo = s;
  }
  /**
   * Get the valid sequence number
   * \returns the valid sequence number
   */
  bool GetValidSeqNo () const
  {
    return m_validSeqNo;
  }
  /**
   * Set the sequence number
   * \param sn the sequence number
   */
  void SetSeqNo (uint32_t sn)
  {
    m_seqNo = sn;
  }
  /**
   * Get the sequence number
   * \returns the sequence number
   */
  uint32_t GetSeqNo () const
  {
    return m_seqNo;
  }
  /**
   * Set the number of hops
   * \param hop the number of hops
   */
  void SetHop (uint16_t hop)
  {
    m_hops = hop;
  }
  /**
   * Get the number of hops
   * \returns the number of hops
   */
  uint16_t GetHop () const
  {
    return m_hops;
  }
  /**
   * Set the lifetime
   * \param lt The lifetime
   */
  void SetLifeTime (Time lt)
  {
    m_lifeTime = lt + Simulator::Now ();
  }
  /**
   * Get the lifetime
   * \returns the lifetime
   */
  Time GetLifeTime () const
  {
    return m_lifeTime - Simulator::Now ();
  }
  /**
   * Set the route flags
   * \param flag the route flags
   */
  void SetFlag (RouteFlags flag)
  {
    m_flag = flag;
  }
  /**
   * Get the route flags
   * \returns the route flags
   */
  RouteFlags GetFlag () const
  {
    return m_flag;
  }
  /**
   * Set the RREQ count
   * \param n the RREQ count
   */
  void SetRreqCnt (uint8_t n)
  {
    m_reqCount = n;
  }
  /**
   * Get the RREQ count
   * \returns the RREQ count
   */
  uint8_t GetRreqCnt () const
  {
    return m_reqCount;
  }
  /**
   * Increment the RREQ count
   */
  void IncrementRreqCnt ()
  {
    m_reqCount++;
  }
  /**
   * Set the unidirectional flag
   * \param u the uni directional flag
   */
  void SetUnidirectional (bool u)
  {
    m_blackListState = u;
  }
  /**
   * Get the unidirectional flag
   * \returns the unidirectional flag
   */
  bool IsUnidirectional () const
  {
    return m_blackListState;
  }
  /**
   * Set the blacklist timeout
   * \param t the blacklist timeout value
   */
  void SetBlacklistTimeout (Time t)
  {
    m_blackListTimeout = t;
  }
  /**
   * Get the blacklist timeout value
   * \returns the blacklist timeout value
   */
  Time GetBlacklistTimeout () const
  {
    return m_blackListTimeout;
  }
  /// RREP_ACK timer
  Timer m_ackTimer;

  /**
   * Set whether destination is an access point
   * \param ap the ap flag
   */
  void SetAccessPoint(bool ap)
  {
    m_accessPoint = ap;
  }
  /**
   * Set whether destination is an access point
   * \return whether entry is access point 
   */
  bool IsAccessPoint() const
  {
    return m_accessPoint;
  }

  /**
   * \brief Compare destination address
   * \param dst IP address to compare
   * \return true if equal
   */
  bool operator== (Ipv6Address const  dst) const
  {
    return (m_ipv6Route->GetDestination () == dst);
  }
  /**
   * Print packet to trace file
   * \param stream The output stream
   * \param unit The time unit to use (default Time::S)
   */
  void Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

private:
  /// Valid Destination Sequence Number flag
  bool m_validSeqNo;
  /// Destination Sequence Number, if m_validSeqNo = true
  uint32_t m_seqNo;
  /// Hop Count (number of hops needed to reach destination)
  uint16_t m_hops;
  /**
  * \brief Expiration or deletion time of the route
  *	Lifetime field in the routing table plays dual role:
  *	for an active route it is the expiration time, and for an invalid route
  *	it is the deletion time.
  */
  Time m_lifeTime;
  /** Ip route, include
   *   - destination address
   *   - source address
   *   - next hop address (gateway)
   *   - output device
   */
  Ptr<Ipv6Route> m_ipv6Route;
  /// Output interface address
  Ipv6InterfaceAddress m_iface;
  /// Routing flags: valid, invalid or in search
  RouteFlags m_flag;

  /// List of precursors
  std::vector<Ipv6Address> m_precursorList;
  /// When I can send another request
  Time m_routeRequestTimout;
  /// Number of route requests
  uint8_t m_reqCount;
  /// Indicate if this entry is in "blacklist"
  bool m_blackListState;
  /// Time for which the node is put into the blacklist
  Time m_blackListTimeout;

  // Indicates whether destination is an internet acess point
  bool m_accessPoint;
};

/**
 * \ingroup madaodv
 * \brief The Routing table used by AODV protocol
 */
class RoutingTable
{
public:
  /**
   * constructor
   * \param t the routing table entry lifetime
   */
  RoutingTable (Time t);
  ///\name Handle lifetime of invalid route
  //\{
  /**
   * Get the lifetime of a bad link
   *
   * \return the lifetime of a bad link
   */
  Time GetBadLinkLifetime () const
  {
    return m_badLinkLifetime;
  }
  /**
   * Set the lifetime of a bad link
   *
   * \param t the lifetime of a bad link
   */
  void SetBadLinkLifetime (Time t)
  {
    m_badLinkLifetime = t;
  }
  //\}
  /**
   * Add routing table entry if it doesn't yet exist in routing table
   * \param r routing table entry
   * \return true in success
   */
  bool AddRoute (RoutingTableEntry & r);
  /**
   * Delete routing table entry with destination address dst, if it exists.
   * \param dst destination address
   * \return true on success
   */
  bool DeleteRoute (Ipv6Address dst);
  /**
   * Lookup routing table entry with destination address dst
   * \param dst destination address
   * \param rt entry with destination address dst, if exists
   * \return true on success
   */
  bool LookupRoute (Ipv6Address dst, RoutingTableEntry & rt);
  /**
   * Lookup route in VALID state
   * \param dst destination address
   * \param rt entry with destination address dst, if exists
   * \return true on success
   */
  bool LookupValidRoute (Ipv6Address dst, RoutingTableEntry & rt);
  /**
   * Update routing table
   * \param rt entry with destination address dst, if exists
   * \return true on success
   */
  bool Update (RoutingTableEntry & rt);
  /**
   * Set routing table entry flags
   * \param dst destination address
   * \param state the routing flags
   * \return true on success
   */
  bool SetEntryState (Ipv6Address dst, RouteFlags state);
  /**
   * Lookup routing entries with next hop Address dst and not empty list of precursors.
   *
   * \param nextHop the next hop IP address
   * \param unreachable
   */
  void GetListOfDestinationWithNextHop (Ipv6Address nextHop, std::map<Ipv6Address, uint32_t> & unreachable);
  /**
   *   Update routing entries with this destination as follows:
   *  1. The destination sequence number of this routing entry, if it
   *     exists and is valid, is incremented.
   *  2. The entry is invalidated by marking the route entry as invalid
   *  3. The Lifetime field is updated to current time plus DELETE_PERIOD.
   *  \param unreachable routes to invalidate
   */
  void InvalidateRoutesWithDst (std::map<Ipv6Address, uint32_t> const & unreachable);
  /**
   * Delete all route from interface with address iface
   * \param iface the interface IP address
   */
  void DeleteAllRoutesFromInterface (Ipv6InterfaceAddress iface);
  /// Delete all entries from routing table
  void Clear ()
  {
    m_ipv6AddressEntry.clear ();
  }
  /// Delete all outdated entries and invalidate valid entry if Lifetime is expired
  void Purge ();

  bool GetDestInSearchOfAp(RoutingTableEntry& entry);

  /** Retrieve active entries with destinations that are access points
   * \param entries - active entries with destinations that are access points
   * \return true if there are one or more entries
   */
  bool ActiveApEntries(RoutingTableEntry& entries);

  /** Mark entry as unidirectional (e.g. add this neighbor to "blacklist" for blacklistTimeout period)
   * \param neighbor - neighbor address link to which assumed to be unidirectional
   * \param blacklistTimeout - time for which the neighboring node is put into the blacklist
   * \return true on success
   */
  bool MarkLinkAsUnidirectional (Ipv6Address neighbor, Time blacklistTimeout);
  /**
   * Print routing table
   * \param stream the output stream
   * \param unit The time unit to use (default Time::S)
   */
  void Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

private:
  /// The routing table
  std::map<Ipv6Address, RoutingTableEntry> m_ipv6AddressEntry;
  /// Deletion time for invalid routes
  Time m_badLinkLifetime;
  /**
   * const version of Purge, for use by Print() method
   * \param table the routing table entry to purge
   */
  void Purge (std::map<Ipv6Address, RoutingTableEntry> &table) const;
};

}  // namespace madaodv
}  // namespace ns3

#endif /* MADAODV_RTABLE_H */
