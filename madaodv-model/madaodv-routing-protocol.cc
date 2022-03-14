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

//mark IS THE KEY WORD, ERASE THEM AT END
#define NS_LOG_APPEND_CONTEXT                                   \
  if (m_ipv6) { std::clog << "[node " << m_ipv6->GetObject<Node> ()->GetId () << "] "; }

#include "madaodv-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/udp-header.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/wifi-mac-queue-item.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <algorithm>
#include <limits>
#include "ns3/output-stream-wrapper.h"
#include "ns3/ndisc-cache.h"
#include "hybrid-wifi-mac.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MadaodvRoutingProtocol");

namespace madaodv {
NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for MADAODV control traffic
const uint32_t RoutingProtocol::MADAODV_PORT = 654;

const char* RoutingProtocol::BROADCAST_ADDR = "ff02::1";

/**
* \ingroup aodv
* \brief Tag used by MADAODV implementation
*/
class DeferredRouteOutputTag : public Tag
{

public:
  /**
   * \brief Constructor
   * \param o the output interface
   */
  DeferredRouteOutputTag (int32_t o = -1) : Tag (),
                                            m_oif (o)
  {
  }

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::madaodv::DeferredRouteOutputTag")
      .SetParent<Tag> ()
      .SetGroupName ("Madaodv")
      .AddConstructor<DeferredRouteOutputTag> ()
    ;
    return tid;
  }

  TypeId  GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  /**
   * \brief Get the output interface
   * \return the output interface
   */
  int32_t GetInterface () const
  {
    return m_oif;
  }

  /**
   * \brief Set the output interface
   * \param oif the output interface
   */
  void SetInterface (int32_t oif)
  {
    m_oif = oif;
  }

  uint32_t GetSerializedSize () const
  {
    return sizeof(int32_t);
  }

  void  Serialize (TagBuffer i) const
  {
    i.WriteU32 (m_oif);
  }

  void  Deserialize (TagBuffer i)
  {
    m_oif = i.ReadU32 ();
  }

  void  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << m_oif;
  }

private:
  /// Positive if output device is fixed in RouteOutput
  int32_t m_oif;
};

NS_OBJECT_ENSURE_REGISTERED (DeferredRouteOutputTag);


//-----------------------------------------------------------------------------
RoutingProtocol::RoutingProtocol ()
  : m_rreqRetries (2),
    m_ttlStart (1),
    m_ttlIncrement (2),
    m_ttlThreshold (7),
    m_timeoutBuffer (2),
    m_rreqRateLimit (10),
    m_rerrRateLimit (10),
    m_activeRouteTimeout (Seconds (3)),
    m_netDiameter (35),
    m_nodeTraversalTime (MilliSeconds (40)),
    m_netTraversalTime (Time ((2 * m_netDiameter) * m_nodeTraversalTime)),
    m_pathDiscoveryTime ( Time (2 * m_netTraversalTime)),
    m_myRouteTimeout (Time (2 * std::max (m_pathDiscoveryTime, m_activeRouteTimeout))),
    m_helloInterval (Seconds (1)),
    m_allowedHelloLoss (2),
    m_deletePeriod (Time (5 * std::max (m_activeRouteTimeout, m_helloInterval))),
    m_nextHopWait (m_nodeTraversalTime + MilliSeconds (10)),
    m_blackListTimeout (Time (m_rreqRetries * m_netTraversalTime)),
    m_maxQueueLen (64),
    m_maxQueueTime (Seconds (30)),
    m_destinationOnly (false),
    m_gratuitousReply (true),
    m_enableHello (false),
    m_routingTable (m_deletePeriod),
    m_queue (m_maxQueueLen, m_maxQueueTime),
    m_requestId (0),
    m_seqNo (0),
    m_rreqIdCache (m_pathDiscoveryTime),
    m_dpd (m_pathDiscoveryTime),
    m_nb (m_helloInterval),
    m_rreqCount (0),
    m_rerrCount (0),
    m_htimer (Timer::CANCEL_ON_DESTROY),
    m_rreqRateLimitTimer (Timer::CANCEL_ON_DESTROY),
    m_rerrRateLimitTimer (Timer::CANCEL_ON_DESTROY),
    m_associatedTimer (Timer::CANCEL_ON_DESTROY),
    m_lastBcastTime (Seconds (0)),
    m_amAccessPoint(false)
{
  m_nb.SetCallback (MakeCallback (&RoutingProtocol::SendRerrWhenBreaksLinkToNextHop, this)); //i think ok
}

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::madaodv::RoutingProtocol")
    .SetParent<Ipv6RoutingProtocol> ()
    .SetGroupName ("Madaodv")
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("HelloInterval", "HELLO messages emission interval.",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&RoutingProtocol::m_helloInterval),
                   MakeTimeChecker ())
    .AddAttribute ("TtlStart", "Initial TTL value for RREQ.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&RoutingProtocol::m_ttlStart),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TtlIncrement", "TTL increment for each attempt using the expanding ring search for RREQ dissemination.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::m_ttlIncrement),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TtlThreshold", "Maximum TTL value for expanding ring search, TTL = NetDiameter is used beyond this value.",
                   UintegerValue (7),
                   MakeUintegerAccessor (&RoutingProtocol::m_ttlThreshold),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TimeoutBuffer", "Provide a buffer for the timeout.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::m_timeoutBuffer),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RreqRetries", "Maximum number of retransmissions of RREQ to discover a route",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::m_rreqRetries),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RreqRateLimit", "Maximum number of RREQ per second.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&RoutingProtocol::m_rreqRateLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RerrRateLimit", "Maximum number of RERR per second.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&RoutingProtocol::m_rerrRateLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NodeTraversalTime", "Conservative estimate of the average one hop traversal time for packets and should include "
                   "queuing delays, interrupt processing times and transfer times.",
                   TimeValue (MilliSeconds (40)),
                   MakeTimeAccessor (&RoutingProtocol::m_nodeTraversalTime),
                   MakeTimeChecker ())
    .AddAttribute ("NextHopWait", "Period of our waiting for the neighbour's RREP_ACK = 10 ms + NodeTraversalTime",
                   TimeValue (MilliSeconds (50)),
                   MakeTimeAccessor (&RoutingProtocol::m_nextHopWait),
                   MakeTimeChecker ())
    .AddAttribute ("ActiveRouteTimeout", "Period of time during which the route is considered to be valid",
                   TimeValue (Seconds (3)),
                   MakeTimeAccessor (&RoutingProtocol::m_activeRouteTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("MyRouteTimeout", "Value of lifetime field in RREP generating by this node = 2 * max(ActiveRouteTimeout, PathDiscoveryTime)",
                   TimeValue (Seconds (11.2)),
                   MakeTimeAccessor (&RoutingProtocol::m_myRouteTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("BlackListTimeout", "Time for which the node is put into the blacklist = RreqRetries * NetTraversalTime",
                   TimeValue (Seconds (5.6)),
                   MakeTimeAccessor (&RoutingProtocol::m_blackListTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("DeletePeriod", "DeletePeriod is intended to provide an upper bound on the time for which an upstream node A "
                   "can have a neighbor B as an active next hop for destination D, while B has invalidated the route to D."
                   " = 5 * max (HelloInterval, ActiveRouteTimeout)",
                   TimeValue (Seconds (15)),
                   MakeTimeAccessor (&RoutingProtocol::m_deletePeriod),
                   MakeTimeChecker ())
    .AddAttribute ("NetDiameter", "Net diameter measures the maximum possible number of hops between two nodes in the network",
                   UintegerValue (35),
                   MakeUintegerAccessor (&RoutingProtocol::m_netDiameter),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NetTraversalTime", "Estimate of the average net traversal time = 2 * NodeTraversalTime * NetDiameter",
                   TimeValue (Seconds (2.8)),
                   MakeTimeAccessor (&RoutingProtocol::m_netTraversalTime),
                   MakeTimeChecker ())
    .AddAttribute ("PathDiscoveryTime", "Estimate of maximum time needed to find route in network = 2 * NetTraversalTime",
                   TimeValue (Seconds (5.6)),
                   MakeTimeAccessor (&RoutingProtocol::m_pathDiscoveryTime),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (64),
                   MakeUintegerAccessor (&RoutingProtocol::SetMaxQueueLen,
                                         &RoutingProtocol::GetMaxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueueTime", "Maximum time packets can be queued (in seconds)",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&RoutingProtocol::SetMaxQueueTime,
                                     &RoutingProtocol::GetMaxQueueTime),
                   MakeTimeChecker ())
    .AddAttribute ("AllowedHelloLoss", "Number of hello messages which may be loss for valid link.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::m_allowedHelloLoss),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("GratuitousReply", "Indicates whether a gratuitous RREP should be unicast to the node originated route discovery.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetGratuitousReplyFlag,
                                        &RoutingProtocol::GetGratuitousReplyFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("DestinationOnly", "Indicates only the destination may respond to this RREQ.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::SetDestinationOnlyFlag,
                                        &RoutingProtocol::GetDestinationOnlyFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableHello", "Indicates whether a hello messages enable.",
                   BooleanValue (false), //mark
                   MakeBooleanAccessor (&RoutingProtocol::SetHelloEnable,
                                        &RoutingProtocol::GetHelloEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableBroadcast", "Indicates whether a broadcast data packets forwarding enable.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetBroadcastEnable,
                                        &RoutingProtocol::GetBroadcastEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("UniformRv",
                   "Access to the underlying UniformRandomVariable",
                   StringValue ("ns3::UniformRandomVariable"),
                   MakePointerAccessor (&RoutingProtocol::m_uniformRandomVariable),
                   MakePointerChecker<UniformRandomVariable> ())
  ;
  return tid;
}

//Not sure what to do with these next two at the moment
void 
RoutingProtocol::NotifyAddRoute	(Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t i, Ipv6Address prefixToUse)
{
 // std::cout << "dst: " << dst << "\t\tnextHop: " << nextHop << "\t\tinterface: " << m_ipv6->GetAddress (i, 0).GetAddress () << std::endl;
 /* if (m_ipv6->GetAddress (i, 0).GetAddress ().IsLinkLocal())
  {
    std::cout << "deleteallroutes" << std::endl;
    m_routingTable.DeleteAllRoutesFromInterface(m_ipv6->GetAddress (i, 0));
  }*/
 NS_LOG_INFO (this << dst << mask << nextHop << i << prefixToUse);
}

void 
RoutingProtocol::NotifyRemoveRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse)
{
  NS_LOG_FUNCTION (this << dst << mask << nextHop << interface);
}

/*bool
RoutingProtocol::AddDevInterface (Ptr<NetDevice> dev)
{
  if (GetInterfaceForDevice(dev) != -1)
  {
    NS_LOG_LOGIC ("device already has registered interface address")
    return false;
  }
  // 1. Ch

  Mac48Address macAddr = dev->GetAddress();



}*/

bool 
RoutingProtocol::InRange (Ipv6Address addr)
{
  NS_LOG_FUNCTION (this);
  static Ipv6Address range ("100::0");
  if (addr.CombinePrefix (Ipv6Prefix (80)) == range)
    {
      return true;
    }
  return false;
}

void
RoutingProtocol::SetMaxQueueLen (uint32_t len)
{
  m_maxQueueLen = len;
  m_queue.SetMaxQueueLen (len);
}
void
RoutingProtocol::SetMaxQueueTime (Time t)
{
  m_maxQueueTime = t;
  m_queue.SetQueueTimeout (t);
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
  m_ipv6 = 0;
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::iterator iter =
         m_socketAddresses.begin (); iter != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
 /* for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::iterator iter =
         m_socketSubnetBroadcastAddresses.begin (); iter != m_socketSubnetBroadcastAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketSubnetBroadcastAddresses.clear ();*/
  Ipv6RoutingProtocol::DoDispose ();
}

void
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << m_ipv6->GetObject<Node> ()->GetId ()
                        << "; Time: " << Now ().As (unit)
                        << ", Local time: " << m_ipv6->GetObject<Node> ()->GetLocalTime ().As (unit)
                        << ", MADAODV Routing table" << std::endl;

  m_routingTable.Print (stream, unit);
  *stream->GetStream () << std::endl;
}

int64_t
RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

void
RoutingProtocol::Start ()
{
  //Print all interface addresses
  /*
  for (auto it = m_socketAddresses.begin(); it != m_socketAddresses.end(); ++it) {
    std::cout << "Socket Address: " << it->second << std::endl;
  }*/
  //Assign node ipv6 address corresponding to mac address
  
  /*Ptr<Node> node = this->GetObject <Node> ();
  Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
  for (uint32_t j = 0; j < ipv6->GetNInterfaces(); j++) {
    for (uint32_t k = 0; k < ipv6->GetNAddresses(j); k++) {
      if (ipv6->GetAddress(j, k) == Ipv6Address("100::9"))
      {
    //    std::cout << "[node " << node->GetId() << "] turning on amaccesspoint" << std::endl;
        SetAmAccessPoint(true);
      }
    }
  }*/

  

  NS_LOG_FUNCTION (this);
  if (m_enableHello)
    {
      m_nb.ScheduleTimer ();
    }
  m_rreqRateLimitTimer.SetFunction (&RoutingProtocol::RreqRateLimitTimerExpire,
                                    this);
  m_rreqRateLimitTimer.Schedule (Seconds (1));

  m_rerrRateLimitTimer.SetFunction (&RoutingProtocol::RerrRateLimitTimerExpire,
                                    this);
  m_rerrRateLimitTimer.Schedule (Seconds (1));

  m_associatedTimer.SetFunction(&RoutingProtocol::CheckAssociated, this);
  m_associatedTimer.Schedule(Seconds(0.001));
  
}

Ptr<Ipv6Route>
RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv6Header &header,
                              Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{

  NS_LOG_FUNCTION (this << header << " space " << (oif ? oif->GetIfIndex () : 0));
  if (!p)
    {
      NS_LOG_DEBUG ("Packet is == 0");
      return LoopbackRoute (header, oif); // later
    }
  if (m_socketAddresses.empty ())
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      NS_LOG_LOGIC ("No madaodv interfaces");
      Ptr<Ipv6Route> route;
      return route;
    }
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<Ipv6Route> route;
  Ipv6Address dst = header.GetDestinationAddress ();
  RoutingTableEntry rt;

  Ptr<Node> node = m_ipv6->GetObject<Node> ();
  std::cout << "[node " << node->GetId() << "] \tsearching for destination " << dst << " in routing table: ";
  if (m_routingTable.LookupValidRoute (dst, rt))
    {
      std::cout << "SUCCESSFUL" << std::endl;
      route = rt.GetRoute ();
      NS_ASSERT (route != 0);
      NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface " << route->GetSource ());
      if (oif != 0 && route->GetOutputDevice () != oif)
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          sockerr = Socket::ERROR_NOROUTETOHOST;
          return Ptr<Ipv6Route> ();
        }

      UpdateRouteLifeTime (dst, m_activeRouteTimeout);
      UpdateRouteLifeTime (route->GetGateway (), m_activeRouteTimeout);
      return route;
    }

  
  else if (OnInternet(dst) && m_routingTable.ActiveApEntries(rt))
  {
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
      PrintRoutingTable (routingStream, Time::Unit::S);
    std::cout << "found an access point in the routing table!" << std::endl;
    std::cout << "route\ndst: " << route->GetDestination() << "\nsource: " << route->GetSource() << "\ngateway: " << route->GetGateway() << std::endl;
  //  std::cout << "route: " << entry.GetRoute() << std::endl;
    //Ptr<Ipv6Route> route = entry.GetRoute();

    NS_ASSERT (route != 0);
    NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface " << route->GetSource ());
    if (oif != 0 && route->GetOutputDevice () != oif)
      {
        NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return Ptr<Ipv6Route> ();
      }

    UpdateRouteLifeTime (dst, m_activeRouteTimeout);
    UpdateRouteLifeTime (route->GetGateway (), m_activeRouteTimeout);
    return route;
  }
  std::cout << "FAILURE" << std::endl;
  // Valid route not found, in this case we return loopback.
  // Actual route request will be deferred until packet will be fully formed,
  // routed to loopback, received from loopback and passed to RouteInput (see below)
  uint32_t iif = (oif ? m_ipv6->GetInterfaceForDevice (oif) : -1);
  DeferredRouteOutputTag tag (iif);
  NS_LOG_DEBUG ("Valid Route not found");
  if (!p->PeekPacketTag (tag))
    {
      p->AddPacketTag (tag);
    }
  return LoopbackRoute (header, oif);
}



void
RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p, const Ipv6Header & header,
                                      UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());

  if (header.GetDestinationAddress().IsLinkLocal())
  {
    return;
  }

  Ipv6Address searchingFor = header.GetDestinationAddress();
  QueueEntry newEntry (p, header, ucb, ecb);

  //If address is on the internet, search set entry to needaccesspoint and set routing table destination as a general "100::" for ap so we aren't searching for many aps
  if (OnInternet(header.GetDestinationAddress()))
  {
    newEntry.SetNeedAccessPoint(true);
    searchingFor = Ipv6Address("100::");
    std::cout << "address " << header.GetDestinationAddress() << " is on internet, searching for 100:: in routing table" << std::endl;
  }


  bool result = m_queue.Enqueue (newEntry);
  
  if (result)
    {
      NS_LOG_LOGIC ("Add packet " << p->GetUid () << " to queue. Protocol " << (uint16_t) header.GetNextHeader ());
      RoutingTableEntry rt;
      
      
      bool result = m_routingTable.LookupRoute (searchingFor, rt);
      std::cout << "result: " << (!result || ((rt.GetFlag () != IN_SEARCH) && result)) << std::endl;
      if (!result || ((rt.GetFlag () != IN_SEARCH) && result))
        {
          NS_LOG_LOGIC ("Send new RREQ for outbound packet to " << header.GetDestinationAddress ());
          //std::cout << "defererd: " << header.GetDestinationAddress () << std::endl;
          SendRequest (header.GetDestinationAddress ());
        }
    }
}

bool
RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv6Header &header,
                             Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                             MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
  Ptr<Node> node = m_ipv6->GetObject<Node> ();
  //std::cout << "[node " << node->GetId() << "] routeinput from " << header.GetSourceAddress() << " to " << header.GetDestinationAddress() << std::endl;

  NS_LOG_FUNCTION (this << p->GetUid () << header.GetDestinationAddress () << idev->GetAddress () << header.GetSourceAddress() << Simulator::Now().GetSeconds());
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No madaodv interfaces");
      return false;
    }
  NS_ASSERT (m_ipv6 != 0);
  NS_ASSERT (p != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv6->GetInterfaceForDevice (idev) >= 0);
  int32_t iif = m_ipv6->GetInterfaceForDevice (idev);

  Ipv6Address dst = header.GetDestinationAddress ();
  Ipv6Address origin = header.GetSourceAddress ();

  // Deferred route request
  if (idev == m_lo)
    {
      DeferredRouteOutputTag tag;
      if (p->PeekPacketTag (tag))
        {
          DeferredRouteOutput (p, header, ucb, ecb);
          return true;
        }
    }

  // Duplicate of own packet
  if (IsMyOwnAddress (origin))
    {
      return true;
    }

  // AODV is not a multicast routing protocol
  if (dst.IsMulticast () && dst != Ipv6Address(BROADCAST_ADDR))
    {
      return false;
    }
  // Broadcast local delivery/forwarding
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv6InterfaceAddress iface = j->second;
      if (m_ipv6->GetInterfaceForAddress (iface.GetAddress ()) == iif)
        {
          if (dst == Ipv6Address(BROADCAST_ADDR))
            {
              if (m_dpd.IsDuplicate (p, header))
                {
                  NS_LOG_DEBUG ("Duplicated packet " << p->GetUid () << " from " << origin << ". Drop.");
                  return true;
                }
              UpdateRouteLifeTime (origin, m_activeRouteTimeout);
              Ptr<Packet> packet = p->Copy ();
              if (lcb.IsNull () == false)
                {
                  NS_LOG_LOGIC ("Broadcast local delivery to " << iface.GetAddress ());
                  lcb (p, header, iif);
                  // Fall through to additional processing
                }
              else
                {
                  NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
                  ecb (p, header, Socket::ERROR_NOROUTETOHOST);
                }
              if (!m_enableBroadcast)
                {
                  return true;
                }
              if (header.GetNextHeader () == UdpL4Protocol::PROT_NUMBER)
                {
                  UdpHeader udpHeader;
                  p->PeekHeader (udpHeader);
                  if (udpHeader.GetDestinationPort () == MADAODV_PORT)
                    {
                      // MADAODV packets sent in broadcast are already managed
                      return true;
                    }
                }
              if (header.GetHopLimit () > 1)
                {
                  NS_LOG_LOGIC ("Forward broadcast. TTL " << (uint16_t) header.GetHopLimit ());
                  RoutingTableEntry toBroadcast;
                  if (m_routingTable.LookupRoute (dst, toBroadcast)) // need a broadcast route
                    {
                      Ptr<Ipv6Route> route = toBroadcast.GetRoute ();
                      ucb (route->GetOutputDevice(), route, packet, header);
                    }
                  else
                    {
                      NS_LOG_DEBUG ("No route to forward broadcast. Drop packet " << p->GetUid ());
                    }
                }
              else
                {
                  NS_LOG_DEBUG ("TTL exceeded. Drop packet " << p->GetUid ());
                }
              return true;
            }
        }
    }

  // Unicast local delivery
  
  bool isDestAddr = false;
  for (uint8_t i = 0; i < m_ipv6->GetNAddresses(iif); i++) 
  {
    if (dst == (m_ipv6->GetAddress(iif, i).GetAddress()))
    {
      isDestAddr = true;
      break;
    }
  }
  if (isDestAddr)
    {
      UpdateRouteLifeTime (origin, m_activeRouteTimeout);
      RoutingTableEntry toOrigin;
      if (m_routingTable.LookupValidRoute (origin, toOrigin))
        {
          UpdateRouteLifeTime (toOrigin.GetNextHop (), m_activeRouteTimeout);
          m_nb.Update (toOrigin.GetNextHop (), m_activeRouteTimeout);
        }
      if (lcb.IsNull () == false)
        {
          NS_LOG_LOGIC ("Unicast local delivery to " << dst);
          lcb (p, header, iif);
        }
      else
        {
          NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
      return true;
    }

  // Check if input device supports IP forwarding
  if (m_ipv6->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }

  // Forwarding
  return Forwarding (p, header, ucb, ecb);
}

bool
RoutingProtocol::Forwarding (Ptr<const Packet> p, const Ipv6Header & header,
                             UnicastForwardCallback ucb, ErrorCallback ecb)
{
 
  NS_LOG_FUNCTION (this);
  Ipv6Address dst = header.GetDestinationAddress ();
  Ipv6Address origin = header.GetSourceAddress ();
  m_routingTable.Purge ();
  RoutingTableEntry toDst;
  if (m_routingTable.LookupRoute (dst, toDst))
    {
    
      if (toDst.GetFlag () == VALID)
        {
          Ptr<Ipv6Route> route = toDst.GetRoute ();
         // std::cout << "\n\nRoute Found:\n Destination: " << route->GetDestination() << "\nSource: " << route->GetSource() << "\nGateway: " << route->GetGateway() << "\nOutput Device: " << route->GetOutputDevice()->GetAddress() << "\n\n" << std::endl;
          NS_LOG_LOGIC (route->GetSource () << " forwarding to " << dst << " from " << origin << " packet " << p->GetUid ());

          /*
           *  Each time a route is used to forward a data packet, its Active Route
           *  Lifetime field of the source, destination and the next hop on the
           *  path to the destination is updated to be no less than the current
           *  time plus ActiveRouteTimeout.
           */
          UpdateRouteLifeTime (origin, m_activeRouteTimeout);
          UpdateRouteLifeTime (dst, m_activeRouteTimeout);
          UpdateRouteLifeTime (route->GetGateway (), m_activeRouteTimeout);
          /*
           *  Since the route between each originator and destination pair is expected to be symmetric, the
           *  Active Route Lifetime for the previous hop, along the reverse path back to the IP source, is also updated
           *  to be no less than the current time plus ActiveRouteTimeout
           */
          RoutingTableEntry toOrigin;
          m_routingTable.LookupRoute (origin, toOrigin);
          UpdateRouteLifeTime (toOrigin.GetNextHop (), m_activeRouteTimeout);

          m_nb.Update (route->GetGateway (), m_activeRouteTimeout);
          m_nb.Update (toOrigin.GetNextHop (), m_activeRouteTimeout);
          //std::cout << "calling ucb" << std::endl;
          ucb (route->GetOutputDevice(), route, p, header);
          return true;
        }
      else
        {
          if (toDst.GetValidSeqNo ())
            {
              SendRerrWhenNoRouteToForward (dst, toDst.GetSeqNo (), origin);
              NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because no route to forward it.");
              return false;
            }
        }
    }
  NS_LOG_LOGIC ("route not found to " << dst << ". Send RERR message.");
  NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because no route to forward it.");
  SendRerrWhenNoRouteToForward (dst, 0, origin);
  return false;
}

void
RoutingProtocol::SetIpv6 (Ptr<Ipv6> ipv6)
{
  NS_ASSERT (ipv6 != 0);
  NS_ASSERT (m_ipv6 == 0);

  m_ipv6 = ipv6;

  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv6->GetNInterfaces () == 1 && m_ipv6->GetAddress (0, 0).GetAddress () == Ipv6Address::GetLoopback ());
  m_lo = m_ipv6->GetNetDevice (0);
 // std::cout << "Loopback Route: " << m_lo << std::endl;
  NS_ASSERT (m_lo != 0);
  // Remember lo route
  RoutingTableEntry rt (/*device=*/ m_lo, /*dst=*/ Ipv6Address::GetLoopback (), /*know seqno=*/ true, /*seqno=*/ 0,
                                    /*iface=*/ Ipv6InterfaceAddress (Ipv6Address::GetLoopback (), Ipv6Prefix(Ipv6Prefix::GetLoopback())),
                                    /*hops=*/ 1, /*next hop=*/ Ipv6Address::GetLoopback (),
                                    /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);

  Simulator::ScheduleNow (&RoutingProtocol::Start, this);
}

void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  
  NS_LOG_FUNCTION (this << m_ipv6->GetAddress (i, 0).GetAddress ());
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  if (l3->GetNAddresses (i) > 1)
    {
      NS_LOG_WARN ("MADAODV does not work with more then one address per each interface.");
    }
  Ptr<NetDevice> dev = l3->GetInterface(i)->GetDevice();

  bool found = false;
  std::map<uint8_t, uint8_t>::iterator itr;
  for (itr = m_addresses.begin(); itr != m_addresses.end(); ++itr) {
      if (itr->first == i) {
        found = true;
        break;
    }
  }

  if (!found)
  {
    Ipv6Address addr = MacToIpv6 (Mac48Address::ConvertFrom(m_ipv6->GetNetDevice(i)->GetAddress()));
    //std::cout << "not found adding addr: " << addr << std::endl;
    bool addrAlreadyAdded = false;
    for (uint8_t j = 0; j < m_ipv6->GetNAddresses(i); j++)
    {
      if (m_ipv6->GetAddress(i, j) == addr)
      {
        addrAlreadyAdded = true;
        break;
      }
    }

    if (!addrAlreadyAdded)
    {
      m_ipv6->AddAddress(i, Ipv6InterfaceAddress(addr));
    }

    m_addresses.insert(std::pair<uint8_t, uint8_t> (i, m_ipv6->GetInterfaceForAddress(addr)));


    Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                             UdpSocketFactory::GetTypeId ());
    NS_ASSERT (socket != 0);

    Ipv6InterfaceAddress iface (addr);
    socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv, this));
    socket->BindToNetDevice (l3->GetNetDevice (i));
    socket->Bind (Inet6SocketAddress (iface.GetAddress (), MADAODV_PORT));
    socket->SetAllowBroadcast (true);
    socket->SetIpv6RecvHopLimit (true);
    m_socketAddresses.insert (std::make_pair (socket, iface));

    // Allow neighbor manager use this interface for layer 2 feedback if possible
    Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
    if (wifi == 0)
      {
        return;
      }
    Ptr<WifiMac> mac = wifi->GetMac ();
    if (mac == 0)
      {
        return;
      }

    mac->TraceConnectWithoutContext ("DroppedMpdu", MakeCallback (&RoutingProtocol::NotifyTxError, this));
  }
 

}

void
RoutingProtocol::NotifyTxError (WifiMacDropReason reason, Ptr<const WifiMacQueueItem> mpdu)
{
  m_nb.GetTxErrorCallback ()(mpdu->GetHeader ());
}

void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv6->GetAddress (i, 0).GetAddress ());

  // Disable layer 2 link state monitoring (if possible)
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  Ptr<NetDevice> dev = l3->GetNetDevice (i);
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
  if (wifi != 0)
    {
      Ptr<WifiMac> mac = wifi->GetMac ()->GetObject<AdhocWifiMac> ();
      if (mac != 0)
        {
          mac->TraceDisconnectWithoutContext ("DroppedMpdu",
                                              MakeCallback (&RoutingProtocol::NotifyTxError, this));
       //   m_nb.DelArpCache (l3->GetInterface (i)->GetArpCache ());
        }
    }

  // Close socket
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv6->GetAddress (i, m_addresses.at(i)));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketAddresses.erase (socket);

  // Close socket
  /*socket = FindSubnetBroadcastSocketWithInterfaceAddress (m_ipv6->GetAddress (i, 0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketSubnetBroadcastAddresses.erase (socket);*/

  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No madaodv interfaces");
      m_htimer.Cancel ();
      m_nb.Clear ();
      m_routingTable.Clear ();
      return;
    }
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv6->GetAddress (i, m_addresses.at(i)));
}

void
RoutingProtocol::NotifyAddAddress (uint32_t i, Ipv6InterfaceAddress address)
{
  //We wont deal with added addresses since we already dealt with the addreses in notifyinterfaceup
  return;
  



 // std::cout << "adding address " << address.GetAddress() << std::endl; 
  NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  if (!l3->IsUp (i))
    {
      return;
    }
  
  if (!InRange(address.GetAddress()))
  {
    //std::cout << "address " << address.GetAddress() << " not in range" << std::endl;
    return;
  }

  //std::cout << "address " << address.GetAddress() << " in range" << std::endl;

  if (l3->GetNAddresses (i)) //mark for change later
    {
      Ptr<NetDevice> dev = l3->GetInterface(i)->GetDevice ();
      Address addr = dev->GetAddress();
      Mac48Address macAddr = Mac48Address::ConvertFrom(addr);

      if (address.GetAddress() != MacToIpv6(macAddr)) // mark get rid of for testing
      {
        return;
      }

      Ipv6InterfaceAddress iface = address; //l3->GetAddress (i, 1); //mark: changed from 0 to 1 <--
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);

      //not getting below
      if (!socket)
        {
          
          if (iface.GetAddress () == Ipv6Address::GetLoopback () || iface.GetAddress().IsLinkLocal()) 
            {
              return;
            }

         // m_address = address.GetAddress(); //mark

          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv,this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (Inet6SocketAddress (iface.GetAddress (), MADAODV_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket, iface));

          // create also a subnet directed broadcast socket
         /* socket = Socket::CreateSocket (GetObject<Node> (),
                                         UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv, this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetBroadcast (), MADAODV_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpRecvTtl (true);
          m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));*/

          // Add local broadcast record to the routing table
     //     Ptr<NetDevice> dev = m_ipv6->GetNetDevice (
     //         m_ipv6->GetInterfaceForAddress (iface.GetAddress ()));
      //    RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true,
      //                                      /*seqno=*/ 0, /*iface=*/ iface, /*hops=*/ 1,
     //                                       /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
      //    m_routingTable.AddRoute (rt);
        }
    }
  else
    {
      NS_LOG_LOGIC ("MADAODV does not work with more then one address per each interface. Ignore added address");
    }
}

void
RoutingProtocol::NotifyRemoveAddress (uint32_t i, Ipv6InterfaceAddress address)
{
  return;


  NS_LOG_FUNCTION (this);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
  if (socket)
    {
      m_routingTable.DeleteAllRoutesFromInterface (address);
      socket->Close ();
      m_socketAddresses.erase (socket);

     /* Ptr<Socket> unicastSocket = FindSubnetBroadcastSocketWithInterfaceAddress (address);
      if (unicastSocket)
        {
          unicastSocket->Close ();
          m_socketAddresses.erase (unicastSocket);
        }*/

      Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
      if (l3->GetNAddresses (i))
        {
          Ipv6InterfaceAddress iface = l3->GetAddress (i, 0);
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv, this));
          // Bind to any IP address so that broadcasts can be received
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (Inet6SocketAddress (iface.GetAddress (), MADAODV_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpv6RecvHopLimit	 (true);
          m_socketAddresses.insert (std::make_pair (socket, iface));

          // create also a unicast socket
         /* socket = Socket::CreateSocket (GetObject<Node> (),
                                         UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv, this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          socket->Bind (InetSocketAddress (iface.GetBroadcast (), MADAODV_PORT));
          socket->SetAllowBroadcast (true);
          socket->SetIpRecvTtl (true);
          m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));*/

          // Add local broadcast record to the routing table
       //   Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (iface.GetAddress ()));
      //    RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true, /*seqno=*/ 0, /*iface=*/ iface,
        //                                    /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
        //  m_routingTable.AddRoute (rt);
        }
      if (m_socketAddresses.empty ())
        {
          NS_LOG_LOGIC ("No madaodv interfaces");
          m_htimer.Cancel ();
          m_nb.Clear ();
          m_routingTable.Clear ();
          return;
        }
    }
  else
    {
      NS_LOG_LOGIC ("Remove address not participating in MADAODV operation");
    }
}

bool
RoutingProtocol::IsMyOwnAddress (Ipv6Address src)
{
  NS_LOG_FUNCTION (this << src);
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv6InterfaceAddress iface = j->second;
      if (src == iface.GetAddress ())
        {
          return true;
        }
    }
  return false;
}

Ptr<Ipv6Route>
RoutingProtocol::LoopbackRoute (const Ipv6Header & hdr, Ptr<NetDevice> oif) const
{
  NS_LOG_FUNCTION (this << hdr);
  NS_ASSERT (m_lo != 0);
  Ptr<Ipv6Route> rt = Create<Ipv6Route> ();
  rt->SetDestination (hdr.GetDestinationAddress ());
  //
  // Source address selection here is tricky.  The loopback route is
  // returned when AODV does not have a route; this causes the packet
  // to be looped back and handled (cached) in RouteInput() method
  // while a route is found. However, connection-oriented protocols
  // like TCP need to create an endpoint four-tuple (src, src port,
  // dst, dst port) and create a pseudo-header for checksumming.  So,
  // AODV needs to guess correctly what the eventual source address
  // will be.
  //
  // For single interface, single address nodes, this is not a problem.
  // When there are possibly multiple outgoing interfaces, the policy
  // implemented here is to pick the first available AODV interface.
  // If RouteOutput() caller specified an outgoing interface, that
  // further constrains the selection of source address
  //
  std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
  if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv6Address addr = j->second.GetAddress ();
          int32_t interface = m_ipv6->GetInterfaceForAddress (addr);
          if (oif == m_ipv6->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
  else
    {
      rt->SetSource (j->second.GetAddress ());
    }
  //std::cout << "valid source: " << rt->GetSource() << std::endl; //mark
  NS_ASSERT_MSG (rt->GetSource () != Ipv6Address (), "Valid MADAODV source address not found");
  rt->SetGateway (Ipv6Address::GetLoopback ());
  rt->SetOutputDevice (m_lo); 
  return rt;
}

void
RoutingProtocol::SendRequest (Ipv6Address dst)
{
  NS_LOG_FUNCTION ( this << dst);
  //std::cout << "SendRequest for  " << dst << std::endl;

  // A node SHOULD NOT originate more than RREQ_RATELIMIT RREQ messages per second.
  if (m_rreqCount == m_rreqRateLimit)
    {
      Simulator::Schedule (m_rreqRateLimitTimer.GetDelayLeft () + MicroSeconds (100),
                           &RoutingProtocol::SendRequest, this, dst);
      return;
    }
  else
    {
      m_rreqCount++;
    }
  // Create RREQ header
  RreqHeader rreqHeader;
  rreqHeader.SetDst (dst);

  //MARK we gotta first check if we have an access point in routing table first should be done in deferredrouteoutput
  Ipv6Address rtSearchFor = dst;

  if (OnInternet(dst))
  {
    std::cout << "set access point query" << std::endl;
    rreqHeader.SetAccessPointQuery(true);
   // rtSearchFor = Ipv6Address("100::");
  }

  RoutingTableEntry rt;
  // Using the Hop field in Routing Table to manage the expanding ring search
  uint16_t ttl = m_ttlStart;
  if (m_routingTable.LookupRoute (rtSearchFor, rt))
    {
      if (rt.GetFlag () != IN_SEARCH)
        {
          ttl = std::min<uint16_t> (rt.GetHop () + m_ttlIncrement, m_netDiameter);
        }
      else
        {
          ttl = rt.GetHop () + m_ttlIncrement;
          if (ttl > m_ttlThreshold)
            {
              ttl = m_netDiameter;
            }
        }
      if (ttl == m_netDiameter)
        {
          rt.IncrementRreqCnt ();
        }
      if (rt.GetValidSeqNo ())
        {
          rreqHeader.SetDstSeqno (rt.GetSeqNo ());
        }
      else
        {
          rreqHeader.SetUnknownSeqno (true);
        }
      rt.SetHop (ttl);
      rt.SetFlag (IN_SEARCH);
      rt.SetLifeTime (m_pathDiscoveryTime);
      m_routingTable.Update (rt);
    }
  else
    {
      rreqHeader.SetUnknownSeqno (true);
      Ptr<NetDevice> dev = 0;
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ rtSearchFor, /*validSeqNo=*/ false, /*seqno=*/ 0,
                                              /*iface=*/ Ipv6InterfaceAddress (),/*hop=*/ ttl,
                                              /*nextHop=*/ Ipv6Address (), /*lifeTime=*/ m_pathDiscoveryTime);
      // Check if TtlStart == NetDiameter
      if (ttl == m_netDiameter)
        {
          newEntry.IncrementRreqCnt ();
        }
      newEntry.SetFlag (IN_SEARCH);

      //Setting this means the destination needs an access point to be reached
      newEntry.SetAccessPoint(true);
      m_routingTable.AddRoute (newEntry);
    }

  if (m_gratuitousReply)
    {
      rreqHeader.SetGratuitousRrep (true);
    }
  if (m_destinationOnly)
    {
      rreqHeader.SetDestinationOnly (true);
    }

  m_seqNo++;
  rreqHeader.SetOriginSeqno (m_seqNo);
  m_requestId++;
  rreqHeader.SetId (m_requestId);

  // Send RREQ as all an all nodes multicast from each interface used by madaodv
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;

      rreqHeader.SetOrigin (iface.GetAddress ());
      //std::cout << "set origin: " << iface.GetAddress() << std::endl;
      m_rreqIdCache.IsDuplicate (iface.GetAddress (), m_requestId);

      Ptr<Packet> packet = Create<Packet> ();
      SocketIpv6HopLimitTag tag;
      tag.SetHopLimit (ttl);
      packet->AddPacketTag (tag);
      packet->AddHeader (rreqHeader);
      TypeHeader tHeader (MADAODVTYPE_RREQ);
      packet->AddHeader (tHeader);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv6Address destination;
      /*
      if (iface.GetMask () == Ipv6Mask::GetOnes ())
        {
          destination = Ipv6Address ("255.255.255.255");
        }
      else
        {
          destination = iface.GetBroadcast ();
        }*/
      destination = Ipv6Address(BROADCAST_ADDR); //mark we gotta check this
      NS_LOG_DEBUG ("Send RREQ with id " << rreqHeader.GetId () << " to socket");
      m_lastBcastTime = Simulator::Now ();

      Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &RoutingProtocol::SendTo, this, socket, packet, destination);
    }
  ScheduleRreqRetry (dst);
}

void
RoutingProtocol::SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv6Address destination)
{
  NS_LOG_FUNCTION(this);
  TypeHeader hdr;
  packet->PeekHeader(hdr);
  std::string type = "";
  Ipv6Address dst;

  if (hdr.Get() == MADAODVTYPE_RREQ) {
    type= "RREQ";
    RreqHeader rhdr;
    packet->PeekHeader(rhdr);
    dst = rhdr.GetDst();
  }

  else if (hdr.Get() == MADAODVTYPE_RREP) {
    type="RREP";
    RrepHeader rhdr;
    packet->PeekHeader(rhdr);
    dst = rhdr.GetOrigin();
  }

  else if (hdr.Get() == MADAODVTYPE_RERR) {
    type="RERR";
  }

  Ptr<Node> node = m_ipv6->GetObject<Node> ();
  std::cout << "[node " << node->GetId() << "] sending " << type << " to destination " << dst << " through " << destination << std::endl;

  socket->SendTo (packet, 0, Inet6SocketAddress (destination, MADAODV_PORT));

}

void
RoutingProtocol::ScheduleRreqRetry (Ipv6Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  if (m_addressReqTimer.find (dst) == m_addressReqTimer.end ())
    {
      Timer timer (Timer::CANCEL_ON_DESTROY);
      m_addressReqTimer[dst] = timer;
    }
  m_addressReqTimer[dst].SetFunction (&RoutingProtocol::RouteRequestTimerExpire, this);
  m_addressReqTimer[dst].Cancel ();
  m_addressReqTimer[dst].SetArguments (dst);
  RoutingTableEntry rt;
  m_routingTable.LookupRoute (dst, rt);
  Time retry;
  if (rt.GetHop () < m_netDiameter)
    {
      retry = 2 * m_nodeTraversalTime * (rt.GetHop () + m_timeoutBuffer);
      std::cout << "retry: " << retry << std::endl;
    }
  else
    {
      NS_ABORT_MSG_UNLESS (rt.GetRreqCnt () > 0, "Unexpected value for GetRreqCount ()");
      uint16_t backoffFactor = rt.GetRreqCnt () - 1;
      NS_LOG_LOGIC ("Applying binary exponential backoff factor " << backoffFactor);
      retry = m_netTraversalTime * (1 << backoffFactor);
    }
  m_addressReqTimer[dst].Schedule (retry);
  NS_LOG_LOGIC ("Scheduled RREQ retry in " << retry.As (Time::S));
}

void
RoutingProtocol::RecvAodv (Ptr<Socket> socket)
{

  NS_LOG_FUNCTION (this << socket);
  Address sourceAddress;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  Inet6SocketAddress inetSourceAddr = Inet6SocketAddress::ConvertFrom (sourceAddress); 
  Ipv6Address sender = inetSourceAddr.GetIpv6 ();
  Ipv6Address receiver;

  if (m_socketAddresses.find (socket) != m_socketAddresses.end ())
    {
      receiver = m_socketAddresses[socket].GetAddress ();
    }
  /*else if (m_socketSubnetBroadcastAddresses.find (socket) != m_socketSubnetBroadcastAddresses.end ())
    {
      receiver = m_socketSubnetBroadcastAddresses[socket].GetLocal ();
    }*/
  else
    {
      NS_ASSERT_MSG (false, "Received a packet from an unknown socket");
    }
  NS_LOG_DEBUG ("MADAODV node " << this << " received a MADAODV packet from " << sender << " to " << receiver);

  UpdateRouteToNeighbor (sender, receiver);
 // Ptr<Node> node = this->GetObject<Node> ();
  //Ptr<Icmpv6L4Protocol> icmp = node->GetObject<Icmpv6L4Protocol> ();
  Ptr<Node> node = this->GetObject<Node> ();
  Ptr<Ipv6L3Protocol> ipv6 = node->GetObject<Ipv6L3Protocol> ();

 /* Ptr<Ipv6Interface> iface = ipv6->GetInterface(ipv6->GetInterfaceForAddress (receiver));
  Ptr<NdiscCache> discCache = iface->GetNdiscCache();
  std::cout << "right above" << std::endl;
  if (!discCache->Lookup(sender))
  {
    std::cout << "adding mac address " << iface->GetDevice()->GetAddress() << " with ipv6 address " << sender << " to disc cache" << std::endl;
    NdiscCache::Entry* entry = discCache->Add(sender);
    entry->SetMacAddress(iface->GetDevice()->GetAddress());
    entry->MarkReachable();
    
  }*/


  TypeHeader tHeader (MADAODVTYPE_RREQ);
  packet->RemoveHeader (tHeader);
  if (!tHeader.IsValid ())
    {
      NS_LOG_DEBUG ("MADAODV message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
      return; // drop
    }
  switch (tHeader.Get ())
    {
    case MADAODVTYPE_RREQ:
      {
        RecvRequest (packet, receiver, sender);
        break;
      }
    case MADAODVTYPE_RREP:
      {
        RecvReply (packet, receiver, sender);
        break;
      }
    case MADAODVTYPE_RERR:
      {
        RecvError (packet, sender);
        break;
      }
    case MADAODVTYPE_RREP_ACK:
      {
        RecvReplyAck (sender);
        break;
      }
    }
}

bool
RoutingProtocol::UpdateRouteLifeTime (Ipv6Address addr, Time lifetime)
{
  NS_LOG_FUNCTION (this << addr << lifetime);
  RoutingTableEntry rt;
  if (m_routingTable.LookupRoute (addr, rt))
    {
      if (rt.GetFlag () == VALID)
        {
          NS_LOG_DEBUG ("Updating VALID route");
          rt.SetRreqCnt (0);
          rt.SetLifeTime (std::max (lifetime, rt.GetLifeTime ()));
          m_routingTable.Update (rt);
          return true;
        }
    }
  return false;
}

void
RoutingProtocol::UpdateRouteToNeighbor (Ipv6Address sender, Ipv6Address receiver)
{
 // std::cout << "sender: " << sender << "\t\treceiver: " << receiver <<"\t\tinterface: " << m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), 1) << std::endl;
  NS_LOG_FUNCTION (this << "sender " << sender << " receiver " << receiver);
  RoutingTableEntry toNeighbor;
  if (!m_routingTable.LookupRoute (sender, toNeighbor))
    {
      Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
      uint8_t iface = m_ipv6->GetInterfaceForAddress (receiver);
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ sender, /*know seqno=*/ false, /*seqno=*/ 0,
                                              /*iface=*/ m_ipv6->GetAddress (iface, m_addresses.at(iface)), //mark 1 was originally a 0
                                              /*hops=*/ 1, /*next hop=*/ sender, /*lifetime=*/ m_activeRouteTimeout);
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
      if (toNeighbor.GetValidSeqNo () && (toNeighbor.GetHop () == 1) && (toNeighbor.GetOutputDevice () == dev))
        {
          toNeighbor.SetLifeTime (std::max (m_activeRouteTimeout, toNeighbor.GetLifeTime ()));
        }
      else
        {
          uint8_t iface = m_ipv6->GetInterfaceForAddress (receiver);
          RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ sender, /*know seqno=*/ false, /*seqno=*/ 0,
                                                  /*iface=*/ m_ipv6->GetAddress (iface, m_addresses.at(iface)),
                                                  /*hops=*/ 1, /*next hop=*/ sender, /*lifetime=*/ std::max (m_activeRouteTimeout, toNeighbor.GetLifeTime ()));
          m_routingTable.Update (newEntry);
        }
    }

}

void
RoutingProtocol::RecvRequest (Ptr<Packet> p, Ipv6Address receiver, Ipv6Address src)
{
  NS_LOG_FUNCTION (this);
  RreqHeader rreqHeader;
  p->RemoveHeader (rreqHeader);


  Ptr<Node> node = m_ipv6->GetObject<Node> ();
  std::cout << "[node " << node->GetId() << "] rreq received from " << src << " for " << rreqHeader.GetDst() << std::endl;

  // A node ignores all RREQs received from any node in its blacklist
  RoutingTableEntry toPrev;
  if (m_routingTable.LookupRoute (src, toPrev))
    {
      if (toPrev.IsUnidirectional ())
        {
          NS_LOG_DEBUG ("Ignoring RREQ from node in blacklist");
          return;
        }
    }

  uint32_t id = rreqHeader.GetId ();
  Ipv6Address origin = rreqHeader.GetOrigin ();

  /*
   *  Node checks to determine whether it has received a RREQ with the same Originator IP Address and RREQ ID.
   *  If such a RREQ has been received, the node silently discards the newly received RREQ.
   */
  if (m_rreqIdCache.IsDuplicate (origin, id))
    {
      NS_LOG_DEBUG ("Ignoring RREQ due to duplicate");
      return;
    }

  // Increment RREQ hop count
  uint8_t hop = rreqHeader.GetHopCount () + 1;
  rreqHeader.SetHopCount (hop);

  /*
   *  When the reverse route is created or updated, the following actions on the route are also carried out:
   *  1. the Originator Sequence Number from the RREQ is compared to the corresponding destination sequence number
   *     in the route table entry and copied if greater than the existing value there
   *  2. the valid sequence number field is set to true;
   *  3. the next hop in the routing table becomes the node from which the  RREQ was received
   *  4. the hop count is copied from the Hop Count in the RREQ message;
   *  5. the Lifetime is set to be the maximum of (ExistingLifetime, MinimalLifetime), where
   *     MinimalLifetime = current time + 2*NetTraversalTime - 2*HopCount*NodeTraversalTime
   */
  RoutingTableEntry toOrigin;
  if (!m_routingTable.LookupRoute (origin, toOrigin))
    {
      //std::cout << "iface: " << m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), 1) << std::endl;
      Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
      uint8_t iface = m_ipv6->GetInterfaceForAddress (receiver);
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ origin, /*validSeno=*/ true, /*seqNo=*/ rreqHeader.GetOriginSeqno (),
                                              /*iface=*/ m_ipv6->GetAddress (iface, m_addresses.at(iface)), /*hops=*/ hop, // mark
                                              /*nextHop*/ src, /*timeLife=*/ Time ((2 * m_netTraversalTime - 2 * hop * m_nodeTraversalTime)));
      m_routingTable.AddRoute (newEntry);
    } 

 // else if (toOrigin.GetInterface().GetAddress().IsLinkLocal())
  //  {
 //     m_routingTable.DeleteRoute(origin);
 //     Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
 //     RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ origin, /*validSeno=*/ true, /*seqNo=*/ rreqHeader.GetOriginSeqno (),
 //                                           /*iface=*/ m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), 1), /*hops=*/ hop, // mark
 //                                           /*nextHop*/ src, /*timeLife=*/ Time ((2 * m_netTraversalTime - 2 * hop * m_nodeTraversalTime)));
 //s     m_routingTable.AddRoute (newEntry);
 //   }

  else
    {
      if (toOrigin.GetValidSeqNo ())
        {
          if (int32_t (rreqHeader.GetOriginSeqno ()) - int32_t (toOrigin.GetSeqNo ()) > 0)
            {
              toOrigin.SetSeqNo (rreqHeader.GetOriginSeqno ());
            }
        }
      else
        {
          toOrigin.SetSeqNo (rreqHeader.GetOriginSeqno ());
        }
      toOrigin.SetValidSeqNo (true);
      toOrigin.SetNextHop (src);
      toOrigin.SetOutputDevice (m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver)));
      uint8_t iface = m_ipv6->GetInterfaceForAddress (receiver);
      toOrigin.SetInterface (m_ipv6->GetAddress (iface, m_addresses.at(iface))); //mark originally a 0, now a 1
      toOrigin.SetHop (hop);
      toOrigin.SetLifeTime (std::max (Time (2 * m_netTraversalTime - 2 * hop * m_nodeTraversalTime),
                                      toOrigin.GetLifeTime ()));
      m_routingTable.Update (toOrigin);
      //m_nb.Update (src, Time (AllowedHelloLoss * HelloInterval));
    }
//Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
    //  PrintRoutingTable (routingStream, Time::Unit::S);
  RoutingTableEntry toNeighbor;
  //std::cout << "recieved from " << m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), m_addresses.at(m_ipv6->GetInterfaceForAddress (receiver))) << std::endl;
  if (!m_routingTable.LookupRoute (src, toNeighbor))
    {
      NS_LOG_DEBUG ("Neighbor:" << src << " not found in routing table. Creating an entry");
      Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
      RoutingTableEntry newEntry (dev, src, false, rreqHeader.GetOriginSeqno (),
                                  m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), m_addresses.at(m_ipv6->GetInterfaceForAddress (receiver))),
                                  1, src, m_activeRouteTimeout);
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      toNeighbor.SetLifeTime (m_activeRouteTimeout);
      toNeighbor.SetValidSeqNo (false);
      toNeighbor.SetSeqNo (rreqHeader.GetOriginSeqno ());
      toNeighbor.SetFlag (VALID);
      toNeighbor.SetOutputDevice (m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver)));
      toNeighbor.SetInterface (m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), m_addresses.at(m_ipv6->GetInterfaceForAddress (receiver))));
      toNeighbor.SetHop (1);
      toNeighbor.SetNextHop (src);
      m_routingTable.Update (toNeighbor);
    }
  m_nb.Update (src, Time (m_allowedHelloLoss * m_helloInterval));

  NS_LOG_LOGIC (receiver << " receive RREQ with hop count " << static_cast<uint32_t> (rreqHeader.GetHopCount ())
                         << " ID " << rreqHeader.GetId ()
                         << " to destination " << rreqHeader.GetDst ());

  //  A node generates a RREP if either:
  //  (i)  it is itself the destination,
  if (IsMyOwnAddress (rreqHeader.GetDst ()))
    {
      m_routingTable.LookupRoute (origin, toOrigin);
      NS_LOG_DEBUG ("Send reply since I am the destination");
      SendReply (rreqHeader, toOrigin);
      return;
    }
  /*
   * (ii) or it has an active route to the destination, the destination sequence number in the node's existing route table entry for the destination
   *      is valid and greater than or equal to the Destination Sequence Number of the RREQ, and the "destination only" flag is NOT set.
   */
  RoutingTableEntry toDst;
  Ipv6Address dst = rreqHeader.GetDst ();

  if (m_routingTable.LookupRoute (dst, toDst))
    {
      
      /*
       * Drop RREQ, This node RREP will make a loop.
       */
      if (toDst.GetNextHop () == src)
        {
          NS_LOG_DEBUG ("Drop RREQ from " << src << ", dest next hop " << toDst.GetNextHop ());
          return;
        }
      /*
       * The Destination Sequence number for the requested destination is set to the maximum of the corresponding value
       * received in the RREQ message, and the destination sequence value currently maintained by the node for the requested destination.
       * However, the forwarding node MUST NOT modify its maintained value for the destination sequence number, even if the value
       * received in the incoming RREQ is larger than the value currently maintained by the forwarding node.
       */
      if ((rreqHeader.GetUnknownSeqno () || (int32_t (toDst.GetSeqNo ()) - int32_t (rreqHeader.GetDstSeqno ()) >= 0))
          && toDst.GetValidSeqNo () )
        {
          if (!rreqHeader.GetDestinationOnly () && toDst.GetFlag () == VALID)
            {
              m_routingTable.LookupRoute (origin, toOrigin);
              SendReplyByIntermediateNode (toDst, toOrigin, rreqHeader.GetGratuitousRrep ());
              return;
            }
          rreqHeader.SetDstSeqno (toDst.GetSeqNo ());
          rreqHeader.SetUnknownSeqno (false);
        }
    }

    //(iii) the rreq is in search of an access point and the node itself is an access point so send back RREP with Ap flag set
    if (rreqHeader.GetAccessPointQuery() && GetAmAccessPoint())
    {
      //std::cout << "[node " << node->GetId() << "] received a RREQ whos APQUERY flag was set an I am an access point" << std::endl;
      //same code as case (i) but just seperated for show
      m_routingTable.LookupRoute (origin, toOrigin);
      NS_LOG_DEBUG ("Send reply since I am an access point");
      SendReply (rreqHeader, toOrigin);
      return;
    }

    //(iiii) the rreq is in search of an access point and the node knows of an access point so back intermediate RREP with ap flag set
    RoutingTableEntry entry;
    if (rreqHeader.GetAccessPointQuery() && m_routingTable.ActiveApEntries(entry))
    {
      /*
       * Drop RREQ, This node RREP will make a loop.
       */
      toDst = entry;
      if (toDst.GetNextHop () == src)
        {
          NS_LOG_DEBUG ("Drop RREQ from " << src << ", dest next hop " << toDst.GetNextHop ());
          return;
        }
      /*
       * The Destination Sequence number for the requested destination is set to the maximum of the corresponding value
       * received in the RREQ message, and the destination sequence value currently maintained by the node for the requested destination.
       * However, the forwarding node MUST NOT modify its maintained value for the destination sequence number, even if the value
       * received in the incoming RREQ is larger than the value currently maintained by the forwarding node.
       */
      if ((rreqHeader.GetUnknownSeqno () || (int32_t (toDst.GetSeqNo ()) - int32_t (rreqHeader.GetDstSeqno ()) >= 0))
          && toDst.GetValidSeqNo () )
        {
          if (!rreqHeader.GetDestinationOnly () && toDst.GetFlag () == VALID)
            {
              m_routingTable.LookupRoute (origin, toOrigin);
              SendReplyByIntermediateNode (toDst, toOrigin, rreqHeader.GetGratuitousRrep ());
              return;
            }
          rreqHeader.SetDstSeqno (toDst.GetSeqNo ());
          rreqHeader.SetUnknownSeqno (false);
        }
    }
  
  SocketIpv6HopLimitTag tag;
  p->RemovePacketTag (tag);
  if (tag.GetHopLimit () < 2)
    {
      NS_LOG_DEBUG ("TTL exceeded. Drop RREQ origin " << src << " destination " << dst );
      return;
    }

  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpv6HopLimitTag ttl;
      ttl.SetHopLimit (tag.GetHopLimit () - 1);
      packet->AddPacketTag (ttl);
      packet->AddHeader (rreqHeader);
      TypeHeader tHeader (MADAODVTYPE_RREQ);
      packet->AddHeader (tHeader);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv6Address destination;
      /*
      if (iface.GetMask () == Ipv6Mask::GetOnes ()) 
        {
          destination = Ipv6Address ("255.255.255.255");
        }
      else
        {
          destination = iface.GetBroadcast ();
        }
      */
      
      /*std::cout << "\n\nRouting Table: " << std::endl;
      Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
      PrintRoutingTable (routingStream, Time::Unit::S);*/

      destination = Ipv6Address(BROADCAST_ADDR);
      m_lastBcastTime = Simulator::Now ();
      Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &RoutingProtocol::SendTo, this, socket, packet, destination);

    }
}

void
RoutingProtocol::SendReply (RreqHeader const & rreqHeader, RoutingTableEntry const & toOrigin)
{
  NS_LOG_FUNCTION (this << toOrigin.GetDestination ());
  //std::cout << "\n\nREPLYING\n\n";
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
  PrintRoutingTable (routingStream, Time::Unit::S);
  /*
   * Destination node MUST increment its own sequence number by one if the sequence number in the RREQ packet is equal to that
   * incremented value. Otherwise, the destination does not change its sequence number before generating the  RREP message.
   */
  if (!rreqHeader.GetUnknownSeqno () && (rreqHeader.GetDstSeqno () == m_seqNo + 1))
    {
      m_seqNo++;
    }
  RrepHeader rrepHeader ( /*prefixSize=*/ 0, /*hops=*/ 0, /*dst=*/ rreqHeader.GetDst (),
                                          /*dstSeqNo=*/ m_seqNo, /*origin=*/ toOrigin.GetDestination (), /*lifeTime=*/ m_myRouteTimeout);
  if (GetAmAccessPoint())
  {
    //std::cout << "setting access point flag in RREP" << std::endl;
    rrepHeader.SetAccessPoint(true);

    uint8_t iface = m_ipv6->GetInterfaceForDevice(toOrigin.GetOutputDevice());

    rrepHeader.SetDst(m_ipv6->GetAddress(iface, m_addresses.at(iface)).GetAddress());
  }

  //std::cout << "ToOrigin:\nDestination: " << toOrigin.GetDestination() << "\nNext Hop: " << toOrigin.GetNextHop() << "\nOutput Interface: " << toOrigin.GetInterface().GetAddress() << "\nOutput Device: " << toOrigin.GetOutputDevice()->GetAddress() << std::endl << std::endl;
  //std::cout << "RREP HEADER:\nDestination: " << rrepHeader.GetDst() << "\nOrigin: " << rrepHeader.GetOrigin() << std::endl;
  Ptr<Packet> packet = Create<Packet> ();
  SocketIpv6HopLimitTag tag;
  tag.SetHopLimit (toOrigin.GetHop ());
  packet->AddPacketTag (tag);
  packet->AddHeader (rrepHeader);
  TypeHeader tHeader (MADAODVTYPE_RREP);
  packet->AddHeader (tHeader);
  //std::cout << "find socket with interface address " << toOrigin.GetInterface() << std::endl;
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toOrigin.GetInterface ());
  NS_ASSERT (socket);


  Ptr<Node> node = m_ipv6->GetObject<Node> ();
  //std::cout << "[node " << node->GetId() << "] sending RREP back to origin " << rrepHeader.GetOrigin () << " from destination " << rrepHeader.GetDst () << " through " << toOrigin.GetNextHop() <<  std::endl;



  socket->SendTo (packet, 0, Inet6SocketAddress (toOrigin.GetNextHop (), MADAODV_PORT)); 
}

void
RoutingProtocol::SendReplyByIntermediateNode (RoutingTableEntry & toDst, RoutingTableEntry & toOrigin, bool gratRep)
{
  NS_LOG_FUNCTION (this);
  RrepHeader rrepHeader (/*prefix size=*/ 0, /*hops=*/ toDst.GetHop (), /*dst=*/ toDst.GetDestination (), /*dst seqno=*/ toDst.GetSeqNo (),
                                          /*origin=*/ toOrigin.GetDestination (), /*lifetime=*/ toDst.GetLifeTime ());
  /* If the node we received a RREQ for is a neighbor we are
   * probably facing a unidirectional link... Better request a RREP-ack
   */
  if (toDst.IsAccessPoint())
  {
    rrepHeader.SetAccessPoint(true);
  }
  if (toDst.GetHop () == 1)
    {
      rrepHeader.SetAckRequired (true);
      RoutingTableEntry toNextHop;
      m_routingTable.LookupRoute (toOrigin.GetNextHop (), toNextHop);
      toNextHop.m_ackTimer.SetFunction (&RoutingProtocol::AckTimerExpire, this);
      toNextHop.m_ackTimer.SetArguments (toNextHop.GetDestination (), m_blackListTimeout);
      toNextHop.m_ackTimer.SetDelay (m_nextHopWait);
    }
  toDst.InsertPrecursor (toOrigin.GetNextHop ());
  toOrigin.InsertPrecursor (toDst.GetNextHop ());
  m_routingTable.Update (toDst);
  m_routingTable.Update (toOrigin);

  Ptr<Packet> packet = Create<Packet> ();
  SocketIpv6HopLimitTag tag;
  tag.SetHopLimit (toOrigin.GetHop ());
  packet->AddPacketTag (tag);
  packet->AddHeader (rrepHeader);
  TypeHeader tHeader (MADAODVTYPE_RREP);
  packet->AddHeader (tHeader);
  //std::cout << "interface: " << toOrigin.GetInterface ().GetAddress() << std::endl;
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toOrigin.GetInterface ());
  Ptr<Node> node = m_ipv6->GetObject<Node> ();
  //std::cout << "here1" << std::endl;
  if (!socket) {
    //std::cout << "node " << node->GetId() << std::endl;
  }
  NS_ASSERT (socket);
  socket->SendTo (packet, 0, Inet6SocketAddress (toOrigin.GetNextHop (), MADAODV_PORT)); 

  // Generating gratuitous RREPs
  if (gratRep)
    {
      RrepHeader gratRepHeader (/*prefix size=*/ 0, /*hops=*/ toOrigin.GetHop (), /*dst=*/ toOrigin.GetDestination (),
                                                 /*dst seqno=*/ toOrigin.GetSeqNo (), /*origin=*/ toDst.GetDestination (),
                                                 /*lifetime=*/ toOrigin.GetLifeTime ());
      Ptr<Packet> packetToDst = Create<Packet> ();
      SocketIpv6HopLimitTag gratTag;
      gratTag.SetHopLimit(toDst.GetHop ());
      packetToDst->AddPacketTag (gratTag);
      packetToDst->AddHeader (gratRepHeader);
      TypeHeader type (MADAODVTYPE_RREP);
      packetToDst->AddHeader (type);
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (toDst.GetInterface ());
      NS_ASSERT (socket);
      NS_LOG_LOGIC ("Send gratuitous RREP " << packet->GetUid ());
      socket->SendTo (packetToDst, 0, Inet6SocketAddress (toDst.GetNextHop (), MADAODV_PORT));
    }
}

void
RoutingProtocol::SendReplyAck (Ipv6Address neighbor)
{
  NS_LOG_FUNCTION (this << " to " << neighbor);
  RrepAckHeader h;
  TypeHeader typeHeader (MADAODVTYPE_RREP_ACK);
  Ptr<Packet> packet = Create<Packet> ();
  SocketIpv6HopLimitTag tag;
  tag.SetHopLimit (1);
  packet->AddPacketTag (tag);
  packet->AddHeader (h);
  packet->AddHeader (typeHeader);
  RoutingTableEntry toNeighbor;
  m_routingTable.LookupRoute (neighbor, toNeighbor);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toNeighbor.GetInterface ());
  NS_ASSERT (socket);
  socket->SendTo (packet, 0, Inet6SocketAddress (neighbor, MADAODV_PORT));
}

void
RoutingProtocol::RecvReply (Ptr<Packet> p, Ipv6Address receiver, Ipv6Address sender)
{
  NS_LOG_FUNCTION (this << " src " << sender);
  RrepHeader rrepHeader;
  p->RemoveHeader (rrepHeader);
  Ptr<Node> node = m_ipv6->GetObject<Node> ();
  std::cout << "\n[node " << node->GetId() << "] rrep received from " << sender << " for " << rrepHeader.GetOrigin() << std::endl;

  Ipv6Address dst = rrepHeader.GetDst ();
  NS_LOG_LOGIC ("RREP destination " << dst << " RREP origin " << rrepHeader.GetOrigin ());

  uint8_t hop = rrepHeader.GetHopCount () + 1;
  rrepHeader.SetHopCount (hop);

  // If RREP is Hello message
  if (dst == rrepHeader.GetOrigin ())
    {
      ProcessHello (rrepHeader, receiver);
      return;
    }

 
  /*
   * If the route table entry to the destination is created or updated, then the following actions occur:
   * -  the route is marked as active,
   * -  the destination sequence number is marked as valid,
   * -  the next hop in the route entry is assigned to be the node from which the RREP is received,
   *    which is indicated by the source IP address field in the IP header,
   * -  the hop count is set to the value of the hop count from RREP message + 1
   * -  the expiry time is set to the current time plus the value of the Lifetime in the RREP message,
   * -  and the destination sequence number is the Destination Sequence Number in the RREP message.
   */

  Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
  RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ dst, /*validSeqNo=*/ true, /*seqno=*/ rrepHeader.GetDstSeqno (),
                                          /*iface=*/ m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), m_addresses.at(m_ipv6->GetInterfaceForAddress (receiver))
                                          ),/*hop=*/ hop,
                                          /*nextHop=*/ sender, /*lifeTime=*/ rrepHeader.GetLifeTime ());

  if (rrepHeader.GetAccessPoint())
  {
    newEntry.SetAccessPoint(true);
  }
  else
  {
    newEntry.SetAccessPoint(false);
  }

  RoutingTableEntry toDst;
  if (m_routingTable.LookupRoute (dst, toDst))
    {
      /*
       * The existing entry is updated only in the following circumstances:
       * (i) the sequence number in the routing table is marked as invalid in route table entry.
       */
      if (!toDst.GetValidSeqNo ())
        {
          m_routingTable.Update (newEntry);
        }
      // (ii)the Destination Sequence Number in the RREP is greater than the node's copy of the destination sequence number and the known value is valid,
      else if ((int32_t (rrepHeader.GetDstSeqno ()) - int32_t (toDst.GetSeqNo ())) > 0)
        {
          m_routingTable.Update (newEntry);
        }
      else
        {
          // (iii) the sequence numbers are the same, but the route is marked as inactive.
          if ((rrepHeader.GetDstSeqno () == toDst.GetSeqNo ()) && (toDst.GetFlag () != VALID))
            {
              m_routingTable.Update (newEntry);
            }
          // (iv)  the sequence numbers are the same, and the New Hop Count is smaller than the hop count in route table entry.
          else if ((rrepHeader.GetDstSeqno () == toDst.GetSeqNo ()) && (hop < toDst.GetHop ()))
            {
              m_routingTable.Update (newEntry);
            }
        }
    }
  else
    {
      // The forward route for this destination is created if it does not already exist.
      NS_LOG_LOGIC ("add new route");
      m_routingTable.AddRoute (newEntry);
    }

  //on receipt of rrep with ap flag set, tupdate all entries currently in search of an ap and set next hop to ap
  RoutingTableEntry entry;
  if (rrepHeader.GetAccessPoint())
  {
    while (m_routingTable.GetDestInSearchOfAp (entry))
    {
      RoutingTableEntry newEntry2 (/*device=*/ dev, /*dst=*/ entry.GetDestination(), /*validSeqNo=*/ true, /*seqno=*/ rrepHeader.GetDstSeqno (),
                                          /*iface=*/ m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), m_addresses.at(m_ipv6->GetInterfaceForAddress (receiver))
                                          ),/*hop=*/ hop,
                                          /*nextHop=*/ dst, /*lifeTime=*/ rrepHeader.GetLifeTime ());

      m_routingTable.Update(newEntry2);
    }
  }



  // Acknowledge receipt of the RREP by sending a RREP-ACK message back
  if (rrepHeader.GetAckRequired ())
    {
      SendReplyAck (sender);
      rrepHeader.SetAckRequired (false);
    }

  //if (dst is an ap), and im origin <-- (is this necessary), dequeue all the packets in search of ap and sendem to ap
  NS_LOG_LOGIC ("receiver " << receiver << " origin " << rrepHeader.GetOrigin ());
  if (IsMyOwnAddress (rrepHeader.GetOrigin ()))
    {
      if (toDst.GetFlag () == IN_SEARCH)
        {
          m_routingTable.Update (newEntry);
          m_addressReqTimer[dst].Cancel ();
          m_addressReqTimer.erase (dst);
        }
      m_routingTable.LookupRoute (dst, toDst);

      if (rrepHeader.GetAccessPoint())
      {
        SendApPacketsFromQueue(toDst.GetRoute());

      }

    //  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
    //  PrintRoutingTable (routingStream, Time::Unit::S);
      
      SendPacketFromQueue (dst, toDst.GetRoute ());
      return;
    }
  
  

  RoutingTableEntry toOrigin;
  if (!m_routingTable.LookupRoute (rrepHeader.GetOrigin (), toOrigin) || toOrigin.GetFlag () == IN_SEARCH)
    {
      return; // Impossible! drop.
    }
  toOrigin.SetLifeTime (std::max (m_activeRouteTimeout, toOrigin.GetLifeTime ()));
  m_routingTable.Update (toOrigin);


  // Update information about precursors
  if (m_routingTable.LookupValidRoute (rrepHeader.GetDst (), toDst))
    {
      toDst.InsertPrecursor (toOrigin.GetNextHop ());
      m_routingTable.Update (toDst);

      RoutingTableEntry toNextHopToDst;
      m_routingTable.LookupRoute (toDst.GetNextHop (), toNextHopToDst);
      toNextHopToDst.InsertPrecursor (toOrigin.GetNextHop ());
      m_routingTable.Update (toNextHopToDst);

      toOrigin.InsertPrecursor (toDst.GetNextHop ());
      m_routingTable.Update (toOrigin);

      RoutingTableEntry toNextHopToOrigin;
      m_routingTable.LookupRoute (toOrigin.GetNextHop (), toNextHopToOrigin);
      toNextHopToOrigin.InsertPrecursor (toDst.GetNextHop ());
      m_routingTable.Update (toNextHopToOrigin);
    }
  SocketIpv6HopLimitTag tag;
  p->RemovePacketTag (tag);
  //mark
  /*if (tag.GetHopLimit () < 2)
    {
      
      NS_LOG_DEBUG ("TTL exceeded. Drop RREP destination " << dst << " origin " << rrepHeader.GetOrigin ());
      return;
    }*/

  Ptr<Packet> packet = Create<Packet> ();
  SocketIpv6HopLimitTag ttl;
  ttl.SetHopLimit (tag.GetHopLimit () - 1);
  packet->AddPacketTag (ttl);
  packet->AddHeader (rrepHeader);
  TypeHeader tHeader (MADAODVTYPE_RREP);
  packet->AddHeader (tHeader);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toOrigin.GetInterface ());
  //std::cout << "socket interface: " << toOrigin.GetInterface() << std::endl;
  NS_ASSERT (socket);

  //std::cout << "[node " << node->GetId() << "] sending RREP to destination " << toOrigin.GetDestination () << " through " << toOrigin.GetNextHop () << std::endl;

  socket->SendTo (packet, 0, Inet6SocketAddress (toOrigin.GetNextHop (), MADAODV_PORT));
}

void
RoutingProtocol::RecvReplyAck (Ipv6Address neighbor)
{
  NS_LOG_FUNCTION (this);
  RoutingTableEntry rt;
  if (m_routingTable.LookupRoute (neighbor, rt))
    {
      rt.m_ackTimer.Cancel ();
      rt.SetFlag (VALID);
      m_routingTable.Update (rt);
    }
}

void
RoutingProtocol::ProcessHello (RrepHeader const & rrepHeader, Ipv6Address receiver )
{
  NS_LOG_FUNCTION (this << "from " << rrepHeader.GetDst ());
  /*
   *  Whenever a node receives a Hello message from a neighbor, the node
   * SHOULD make sure that it has an active route to the neighbor, and
   * create one if necessary.
   */
  RoutingTableEntry toNeighbor;
  if (!m_routingTable.LookupRoute (rrepHeader.GetDst (), toNeighbor))
    {
      Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ rrepHeader.GetDst (), /*validSeqNo=*/ true, /*seqno=*/ rrepHeader.GetDstSeqno (),
                                              /*iface=*/ m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), m_addresses.at(m_ipv6->GetInterfaceForAddress (receiver))),
                                              /*hop=*/ 1, /*nextHop=*/ rrepHeader.GetDst (), /*lifeTime=*/ rrepHeader.GetLifeTime ());
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      toNeighbor.SetLifeTime (std::max (Time (m_allowedHelloLoss * m_helloInterval), toNeighbor.GetLifeTime ()));
      toNeighbor.SetSeqNo (rrepHeader.GetDstSeqno ());
      toNeighbor.SetValidSeqNo (true);
      toNeighbor.SetFlag (VALID);
      toNeighbor.SetOutputDevice (m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver)));
      toNeighbor.SetInterface (m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), m_addresses.at(m_ipv6->GetInterfaceForAddress (receiver))));
      toNeighbor.SetHop (1);
      toNeighbor.SetNextHop (rrepHeader.GetDst ());
      m_routingTable.Update (toNeighbor);
    }
  if (m_enableHello)
    {
      m_nb.Update (rrepHeader.GetDst (), Time (m_allowedHelloLoss * m_helloInterval));
    }
}

void
RoutingProtocol::RecvError (Ptr<Packet> p, Ipv6Address src )
{
  NS_LOG_FUNCTION (this << " from " << src);
  RerrHeader rerrHeader;
  p->RemoveHeader (rerrHeader);
  std::map<Ipv6Address, uint32_t> dstWithNextHopSrc;
  std::map<Ipv6Address, uint32_t> unreachable;
  m_routingTable.GetListOfDestinationWithNextHop (src, dstWithNextHopSrc);
  std::pair<Ipv6Address, uint32_t> un;
  while (rerrHeader.RemoveUnDestination (un))
    {
      for (std::map<Ipv6Address, uint32_t>::const_iterator i =
             dstWithNextHopSrc.begin (); i != dstWithNextHopSrc.end (); ++i)
        {
          if (i->first == un.first)
            {
              unreachable.insert (un);
            }
        }
    }

  std::vector<Ipv6Address> precursors;
  for (std::map<Ipv6Address, uint32_t>::const_iterator i = unreachable.begin ();
       i != unreachable.end (); )
    {
      if (!rerrHeader.AddUnDestination (i->first, i->second))
        {
          TypeHeader typeHeader (MADAODVTYPE_RERR);
          Ptr<Packet> packet = Create<Packet> ();
          SocketIpv6HopLimitTag tag;
          tag.SetHopLimit (1);
          packet->AddPacketTag (tag);
          packet->AddHeader (rerrHeader);
          packet->AddHeader (typeHeader);
          SendRerrMessage (packet, precursors);
          rerrHeader.Clear ();
        }
      else
        {
          RoutingTableEntry toDst;
          m_routingTable.LookupRoute (i->first, toDst);
          toDst.GetPrecursors (precursors);
          ++i;
        }
    }
  if (rerrHeader.GetDestCount () != 0)
    {
      TypeHeader typeHeader (MADAODVTYPE_RERR);
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpv6HopLimitTag tag;
      tag.SetHopLimit (1);
      packet->AddPacketTag (tag);
      packet->AddHeader (rerrHeader);
      packet->AddHeader (typeHeader);
      SendRerrMessage (packet, precursors);
    }
  m_routingTable.InvalidateRoutesWithDst (unreachable);
}

void
RoutingProtocol::RouteRequestTimerExpire (Ipv6Address dst)
{
  NS_LOG_LOGIC (this);
  RoutingTableEntry toDst;
  if (m_routingTable.LookupValidRoute (dst, toDst))
    {
      SendPacketFromQueue (dst, toDst.GetRoute ());
      NS_LOG_LOGIC ("route to " << dst << " found");
      return;
    }
  /*
   *  If a route discovery has been attempted RreqRetries times at the maximum TTL without
   *  receiving any RREP, all data packets destined for the corresponding destination SHOULD be
   *  dropped from the buffer and a Destination Unreachable message SHOULD be delivered to the application.
   */
  if (toDst.GetRreqCnt () == m_rreqRetries)
    {
      NS_LOG_LOGIC ("route discovery to " << dst << " has been attempted RreqRetries (" << m_rreqRetries << ") times with ttl " << m_netDiameter);
      m_addressReqTimer.erase (dst);
      m_routingTable.DeleteRoute (dst);
      NS_LOG_DEBUG ("Route not found. Drop all packets with dst " << dst);
      m_queue.DropPacketWithDst (dst);
      return;
    }

  if (toDst.GetFlag () == IN_SEARCH)
    {
      NS_LOG_LOGIC ("Resend RREQ to " << dst << " previous ttl " << toDst.GetHop ());
      SendRequest (dst);
    }
  else
    {
      NS_LOG_DEBUG ("Route down. Stop search. Drop packet with destination " << dst);
      m_addressReqTimer.erase (dst);
      m_routingTable.DeleteRoute (dst);
      m_queue.DropPacketWithDst (dst);
    }
}

void
RoutingProtocol::HelloTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  Time offset = Time (Seconds (0));
  if (m_lastBcastTime > Time (Seconds (0)))
    {
      offset = Simulator::Now () - m_lastBcastTime;
      NS_LOG_DEBUG ("Hello deferred due to last bcast at:" << m_lastBcastTime);
    }
  else
    {
      SendHello ();
    }
  m_htimer.Cancel ();
  Time diff = m_helloInterval - offset;
  m_htimer.Schedule (std::max (Time (Seconds (0)), diff));
  m_lastBcastTime = Time (Seconds (0));
}

void
RoutingProtocol::RreqRateLimitTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  m_rreqCount = 0;
  m_rreqRateLimitTimer.Schedule (Seconds (1));
}

void
RoutingProtocol::RerrRateLimitTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  m_rerrCount = 0;
  m_rerrRateLimitTimer.Schedule (Seconds (1));
}

void
RoutingProtocol::AckTimerExpire (Ipv6Address neighbor, Time blacklistTimeout)
{
  NS_LOG_FUNCTION (this);
  m_routingTable.MarkLinkAsUnidirectional (neighbor, blacklistTimeout);
}

Mac48Address 
RoutingProtocol::Ipv6ToMac (Ipv6Address ipv6Addr)
{
  Mac48Address macAddr;
  uint8_t ipv6Buffer[16];
  ipv6Addr.GetBytes(ipv6Buffer);

  uint8_t macBuffer[6];

  // We want bytes 11-16 (where the stars in: 100:0:0:0:0:*:*:*)
  for (uint8_t i = 0; i < 6; i++)
  {
    macBuffer[(int)i] = ipv6Buffer[(int)i+10];  
  }

  macAddr.CopyFrom((const uint8_t*) macBuffer);

  return macAddr;
}

Ipv6Address 
RoutingProtocol::MacToIpv6 (Mac48Address macAddr)
{
  uint8_t macBuffer[6];
  macAddr.CopyTo(macBuffer);

  uint8_t ipv6Buffer[16];
  ipv6Buffer[0] = 1;
  for (uint8_t i = 1; i < 10; i++)
  {
    ipv6Buffer[i] = 0;
  }

  for (uint8_t i = 10; i < 16; i++)
  {
    ipv6Buffer[i] = macBuffer[i-10];
  }

  Ipv6Address ipv6Addr;
  ipv6Addr.Set (ipv6Buffer);
  return ipv6Addr;
}

bool 
RoutingProtocol::OnInternet (Ipv6Address addr)
{
  return !(InRange(addr));
}
int8_t 
RoutingProtocol::GetIndexForAddress (uint8_t i, Ipv6Address addr)
{
  for (uint8_t j = 0; j < m_ipv6->GetNAddresses(i); j++)
  {
    if (m_ipv6->GetAddress(i, j) == addr)
    {
      return j;
    }
  }

  return -1;
}

void
RoutingProtocol::SendHello ()
{
  NS_LOG_FUNCTION (this);
  /* Broadcast a RREP with TTL = 1 with the RREP message fields set as follows:
   *   Destination IP Address         The node's IP address.
   *   Destination Sequence Number    The node's latest sequence number.
   *   Hop Count                      0
   *   Lifetime                       AllowedHelloLoss * HelloInterval
   */
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;
      RrepHeader helloHeader (/*prefix size=*/ 0, /*hops=*/ 0, /*dst=*/ iface.GetAddress (), /*dst seqno=*/ m_seqNo,
                                               /*origin=*/ iface.GetAddress (),/*lifetime=*/ Time (m_allowedHelloLoss * m_helloInterval));
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpv6HopLimitTag tag;
      tag.SetHopLimit (1);
      packet->AddPacketTag (tag);
      packet->AddHeader (helloHeader);
      TypeHeader tHeader (MADAODVTYPE_RREP);
      packet->AddHeader (tHeader);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv6Address destination;
   /*   if (iface.GetMask () == Ipv6Mask::GetOnes ())
        {
          destination = Ipv6Address ("255.255.255.255"); mark
        }
      else
        {
          destination = iface.GetBroadcast ();
        }*/
      destination = Ipv6Address(BROADCAST_ADDR);
      Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10)));
      Simulator::Schedule (jitter, &RoutingProtocol::SendTo, this, socket, packet, destination);
    }
}

void
RoutingProtocol::SendApPacketsFromQueue (Ptr<Ipv6Route> route)
{
  //std::cout << "sending out ap packets from queue" << std::endl;
  //std::cout << "route: \ndestination: " << route->GetDestination() << "\nGateway: " << route->GetGateway() << "\nSource: " << route->GetSource() << "\nGateway: " << route->GetGateway() << std::endl;
  NS_LOG_FUNCTION (this);
  QueueEntry queueEntry;
  while (m_queue.DequeueApQuery (queueEntry))
    {
      //mark, later on were gonna need to use the destination listed in the entry to determine what address to go to on the internet
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag)
          && tag.GetInterface () != -1
          && tag.GetInterface () != m_ipv6->GetInterfaceForDevice (route->GetOutputDevice ()))
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          return;
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv6Header header = queueEntry.GetIpv6Header ();
      header.SetSourceAddress (route->GetSource ());
      header.SetHopLimit (header.GetHopLimit () + 1); // compensate extra TTL decrement by fake loopback routing

      header.SetDestinationAddress(route->GetDestination()); //mark we gotta make it so the original destination addrss is preserved someway
      
      ucb (route->GetOutputDevice(), route, p, header);
    }
}

void
RoutingProtocol::SendPacketFromQueue (Ipv6Address dst, Ptr<Ipv6Route> route)
{
  
  NS_LOG_FUNCTION (this);
  QueueEntry queueEntry;
  while (m_queue.Dequeue (dst, queueEntry))
    {
     
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag)
          && tag.GetInterface () != -1
          && tag.GetInterface () != m_ipv6->GetInterfaceForDevice (route->GetOutputDevice ()))
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          return;
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv6Header header = queueEntry.GetIpv6Header ();
      header.SetSourceAddress (route->GetSource ());
      header.SetHopLimit (header.GetHopLimit () + 1); // compensate extra TTL decrement by fake loopback routing
      
      ucb (route->GetOutputDevice(), route, p, header);
    }
}

void
RoutingProtocol::SendRerrWhenBreaksLinkToNextHop (Ipv6Address nextHop)
{
  NS_LOG_FUNCTION (this << nextHop);
  RerrHeader rerrHeader;
  std::vector<Ipv6Address> precursors;
  std::map<Ipv6Address, uint32_t> unreachable;

  RoutingTableEntry toNextHop;
  if (!m_routingTable.LookupRoute (nextHop, toNextHop))
    {
      return;
    }
  toNextHop.GetPrecursors (precursors);
  rerrHeader.AddUnDestination (nextHop, toNextHop.GetSeqNo ());
  m_routingTable.GetListOfDestinationWithNextHop (nextHop, unreachable);
  for (std::map<Ipv6Address, uint32_t>::const_iterator i = unreachable.begin (); i
       != unreachable.end (); )
    {
      if (!rerrHeader.AddUnDestination (i->first, i->second))
        {
          NS_LOG_LOGIC ("Send RERR message with maximum size.");
          TypeHeader typeHeader (MADAODVTYPE_RERR);
          Ptr<Packet> packet = Create<Packet> ();
          SocketIpv6HopLimitTag tag;
          tag.SetHopLimit (1);
          packet->AddPacketTag (tag);
          packet->AddHeader (rerrHeader);
          packet->AddHeader (typeHeader);
          SendRerrMessage (packet, precursors);
          rerrHeader.Clear ();
        }
      else
        {
          RoutingTableEntry toDst;
          m_routingTable.LookupRoute (i->first, toDst);
          toDst.GetPrecursors (precursors);
          ++i;
        }
    }
  if (rerrHeader.GetDestCount () != 0)
    {
      TypeHeader typeHeader (MADAODVTYPE_RERR);
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpv6HopLimitTag tag;
      tag.SetHopLimit (1);
      packet->AddPacketTag (tag);
      packet->AddHeader (rerrHeader);
      packet->AddHeader (typeHeader);
      SendRerrMessage (packet, precursors);
    }
  unreachable.insert (std::make_pair (nextHop, toNextHop.GetSeqNo ()));
  m_routingTable.InvalidateRoutesWithDst (unreachable);
}

void
RoutingProtocol::SendRerrWhenNoRouteToForward (Ipv6Address dst,
                                               uint32_t dstSeqNo, Ipv6Address origin)
{
  NS_LOG_FUNCTION (this);
  // A node SHOULD NOT originate more than RERR_RATELIMIT RERR messages per second.
  if (m_rerrCount == m_rerrRateLimit)
    {
      // Just make sure that the RerrRateLimit timer is running and will expire
      NS_ASSERT (m_rerrRateLimitTimer.IsRunning ());
      // discard the packet and return
      NS_LOG_LOGIC ("RerrRateLimit reached at " << Simulator::Now ().As (Time::S) << " with timer delay left "
                                                << m_rerrRateLimitTimer.GetDelayLeft ().As (Time::S)
                                                << "; suppressing RERR");
      return;
    }
  RerrHeader rerrHeader;
  rerrHeader.AddUnDestination (dst, dstSeqNo);
  RoutingTableEntry toOrigin;
  Ptr<Packet> packet = Create<Packet> ();
  SocketIpv6HopLimitTag tag;
  tag.SetHopLimit (1);
  packet->AddPacketTag (tag);
  packet->AddHeader (rerrHeader);
  packet->AddHeader (TypeHeader (MADAODVTYPE_RERR));
  if (m_routingTable.LookupValidRoute (origin, toOrigin))
    {
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (
          toOrigin.GetInterface ());
      NS_ASSERT (socket);
      NS_LOG_LOGIC ("Unicast RERR to the source of the data transmission");
      socket->SendTo (packet, 0, Inet6SocketAddress (toOrigin.GetNextHop (), MADAODV_PORT));
    }
  else
    {
      for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator i =
             m_socketAddresses.begin (); i != m_socketAddresses.end (); ++i)
        {
          Ptr<Socket> socket = i->first;
          Ipv6InterfaceAddress iface = i->second;
          NS_ASSERT (socket);
          NS_LOG_LOGIC ("Broadcast RERR message from interface " << iface.GetAddress ());
          // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
          Ipv6Address destination;
       /*   if (iface.GetMask () == Ipv6Mask::GetOnes ())
            {
              destination = Ipv6Address ("255.255.255.255");
            }
          else
            {
              destination = iface.GetBroadcast ();
            }*/
          destination = Ipv6Address(BROADCAST_ADDR);
          socket->SendTo (packet->Copy (), 0, Inet6SocketAddress (destination, MADAODV_PORT));
        }
    }
}

void
RoutingProtocol::SendRerrMessage (Ptr<Packet> packet, std::vector<Ipv6Address> precursors)
{
  NS_LOG_FUNCTION (this);

  if (precursors.empty ())
    {
      NS_LOG_LOGIC ("No precursors");
      return;
    }
  // A node SHOULD NOT originate more than RERR_RATELIMIT RERR messages per second.
  if (m_rerrCount == m_rerrRateLimit)
    {
      // Just make sure that the RerrRateLimit timer is running and will expire
      NS_ASSERT (m_rerrRateLimitTimer.IsRunning ());
      // discard the packet and return
      NS_LOG_LOGIC ("RerrRateLimit reached at " << Simulator::Now ().As (Time::S) << " with timer delay left "
                                                << m_rerrRateLimitTimer.GetDelayLeft ().As (Time::S)
                                                << "; suppressing RERR");
      return;
    }
  // If there is only one precursor, RERR SHOULD be unicast toward that precursor
  if (precursors.size () == 1)
    {
      RoutingTableEntry toPrecursor;
      if (m_routingTable.LookupValidRoute (precursors.front (), toPrecursor))
        {
          Ptr<Socket> socket = FindSocketWithInterfaceAddress (toPrecursor.GetInterface ());
          NS_ASSERT (socket);
          NS_LOG_LOGIC ("one precursor => unicast RERR to " << toPrecursor.GetDestination () << " from " << toPrecursor.GetInterface ().GetAddress ());
          Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &RoutingProtocol::SendTo, this, socket, packet, precursors.front ());
          m_rerrCount++;
        }
      return;
    }

  //  Should only transmit RERR on those interfaces which have precursor nodes for the broken route
  std::vector<Ipv6InterfaceAddress> ifaces;
  RoutingTableEntry toPrecursor;
  for (std::vector<Ipv6Address>::const_iterator i = precursors.begin (); i != precursors.end (); ++i)
    {
      if (m_routingTable.LookupValidRoute (*i, toPrecursor)
          && std::find (ifaces.begin (), ifaces.end (), toPrecursor.GetInterface ()) == ifaces.end ())
        {
          ifaces.push_back (toPrecursor.GetInterface ());
        }
    }

  for (std::vector<Ipv6InterfaceAddress>::const_iterator i = ifaces.begin (); i != ifaces.end (); ++i)
    {
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (*i);
      NS_ASSERT (socket);
      NS_LOG_LOGIC ("Broadcast RERR message from interface " << i->GetAddress ());
      // std::cout << "Broadcast RERR message from interface " << i->GetAddress () << std::endl;
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ptr<Packet> p = packet->Copy ();
      Ipv6Address destination;
      /*if (i->GetMask () == Ipv6Mask::GetOnes ())
        {
          destination = Ipv6Address ("255.255.255.255");
        }
      else
        {
          destination = i->GetBroadcast ();
        }*/
      destination = Ipv6Address(BROADCAST_ADDR);
      Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &RoutingProtocol::SendTo, this, socket, p, destination);
    }
}

Ptr<Socket>
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv6InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;
      //std::cout << "iface addr: " << iface.GetAddress() << "\t\tprefix: " << iface.GetPrefix() << "\t\tstate: " << iface.GetState() << "\t\tScope: " << iface.GetScope() << std::endl;
      //std::cout << "addr addr: " << addr.GetAddress() << "\t\tprefix: " << addr.GetPrefix() << "\t\tstate: " << addr.GetState() << "\t\tScope: " << addr.GetScope() << std::endl;
      //std::cout << "equal: " << (iface == addr) << std::endl;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

/*Ptr<Socket>
RoutingProtocol::FindSubnetBroadcastSocketWithInterfaceAddress (Ipv6InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketSubnetBroadcastAddresses.begin (); j != m_socketSubnetBroadcastAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}*/

void
RoutingProtocol::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  uint32_t startTime;

  if (m_enableHello)
    {
      m_htimer.SetFunction (&RoutingProtocol::HelloTimerExpire, this);
      startTime = m_uniformRandomVariable->GetInteger (0, 100);
      NS_LOG_DEBUG ("Starting at time " << startTime << "ms");
      m_htimer.Schedule (MilliSeconds (startTime));
    }

  Ipv6RoutingProtocol::DoInitialize ();
}

void
RoutingProtocol::CheckAssociated ()
{
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  for (uint32_t i = 0; i < l3->GetNInterfaces (); i++)
  {
    Ptr<NetDevice> dev = l3->GetInterface(i)->GetDevice();
    Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();

    if (wifi == 0)
      {
        continue;
      }

    Ptr<WifiMac> mac = wifi->GetMac ();
    Ptr<HybridWifiMac> hybrid = mac->GetObject<HybridWifiMac> ();
Ptr<Node> node = this->GetObject<Node> ();
    if (hybrid == 0)
      {
     //   std::cout << "[node " << node->GetId() << "] not hybrid" << std::endl;
        continue;
      }
    
    if (hybrid->IsAssociated())
    {
      Ptr<Node> node = this->GetObject<Node> ();
   //  std::cout << "[node " << node->GetId() << "] set associated" << std::endl;
      SetAmAccessPoint(true);
      m_associatedTimer.SetFunction(&RoutingProtocol::CheckAssociated, this);
      m_associatedTimer.Schedule(Seconds(0.01));
      return;
    }
  }
  Ptr<Node> node = this->GetObject<Node> ();
 // std::cout << "[node " << node->GetId() << "] set not associated" << std::endl;
  SetAmAccessPoint(false);
  m_associatedTimer.SetFunction(&RoutingProtocol::CheckAssociated, this);
  m_associatedTimer.Schedule(Seconds(0.01));
}

} //namespace madaodv
} //namespace ns3