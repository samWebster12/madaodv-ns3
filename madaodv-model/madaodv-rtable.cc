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

#include "madaodv-rtable.h"
#include <algorithm>
#include <iomanip>
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MadaodvRoutingTable");

namespace madaodv {

/*
 The Routing Table
 */

RoutingTableEntry::RoutingTableEntry (Ptr<NetDevice> dev, Ipv6Address dst, bool vSeqNo, uint32_t seqNo,
                                      Ipv6InterfaceAddress iface, uint16_t hops, Ipv6Address nextHop, Time lifetime)
  : m_ackTimer (Timer::CANCEL_ON_DESTROY),
    m_validSeqNo (vSeqNo),
    m_seqNo (seqNo),
    m_hops (hops),
    m_lifeTime (lifetime + Simulator::Now ()),
    m_iface (iface),
    m_flag (VALID),
    m_reqCount (0),
    m_blackListState (false),
    m_blackListTimeout (Simulator::Now ()),
    m_accessPoint(false)
{
  m_ipv6Route = Create<Ipv6Route> ();
  m_ipv6Route->SetDestination (dst);
  m_ipv6Route->SetGateway (nextHop);
  m_ipv6Route->SetSource (m_iface.GetAddress ());
  m_ipv6Route->SetOutputDevice (dev);
}

RoutingTableEntry::~RoutingTableEntry ()
{
}

bool
RoutingTableEntry::InsertPrecursor (Ipv6Address id)
{
  NS_LOG_FUNCTION (this << id);
  if (!LookupPrecursor (id))
    {
      m_precursorList.push_back (id);
      return true;
    }
  else
    {
      return false;
    }
}

bool
RoutingTableEntry::LookupPrecursor (Ipv6Address id)
{
  NS_LOG_FUNCTION (this << id);
  for (std::vector<Ipv6Address>::const_iterator i = m_precursorList.begin (); i
       != m_precursorList.end (); ++i)
    {
      if (*i == id)
        {
          NS_LOG_LOGIC ("Precursor " << id << " found");
          return true;
        }
    }
  NS_LOG_LOGIC ("Precursor " << id << " not found");
  return false;
}

bool
RoutingTableEntry::DeletePrecursor (Ipv6Address id)
{
  NS_LOG_FUNCTION (this << id);
  std::vector<Ipv6Address>::iterator i = std::remove (m_precursorList.begin (),
                                                      m_precursorList.end (), id);
  if (i == m_precursorList.end ())
    {
      NS_LOG_LOGIC ("Precursor " << id << " not found");
      return false;
    }
  else
    {
      NS_LOG_LOGIC ("Precursor " << id << " found");
      m_precursorList.erase (i, m_precursorList.end ());
    }
  return true;
}

void
RoutingTableEntry::DeleteAllPrecursors ()
{
  NS_LOG_FUNCTION (this);
  m_precursorList.clear ();
}

bool
RoutingTableEntry::IsPrecursorListEmpty () const
{
  return m_precursorList.empty ();
}

void
RoutingTableEntry::GetPrecursors (std::vector<Ipv6Address> & prec) const
{
  NS_LOG_FUNCTION (this);
  if (IsPrecursorListEmpty ())
    {
      return;
    }
  for (std::vector<Ipv6Address>::const_iterator i = m_precursorList.begin (); i
       != m_precursorList.end (); ++i)
    {
      bool result = true;
      for (std::vector<Ipv6Address>::const_iterator j = prec.begin (); j
           != prec.end (); ++j)
        {
          if (*j == *i)
            {
              result = false;
            }
        }
      if (result)
        {
          prec.push_back (*i);
        }
    }
}

void
RoutingTableEntry::Invalidate (Time badLinkLifetime)
{
  NS_LOG_FUNCTION (this << badLinkLifetime.As (Time::S));
  if (m_flag == INVALID)
    {
      return;
    }
  m_flag = INVALID;
  m_reqCount = 0;
  m_lifeTime = badLinkLifetime + Simulator::Now ();
}

void
RoutingTableEntry::Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit /* = Time::S */) const
{
  std::ostream* os = stream->GetStream ();
  // Copy the current ostream state
  std::ios oldState (nullptr);
  oldState.copyfmt (*os);

  *os << std::resetiosflags (std::ios::adjustfield) << std::setiosflags (std::ios::left);

  std::ostringstream dest, gw, iface, expire;
  dest << m_ipv6Route->GetDestination ();
  gw << m_ipv6Route->GetGateway ();
  iface << m_iface.GetAddress ();
  expire << std::setprecision (2) << (m_lifeTime - Simulator::Now ()).As (unit);
  *os << std::setw (16) << dest.str();
  *os << std::setw (16) << gw.str();
  *os << std::setw (16) << iface.str();
  *os << std::setw (16);
  switch (m_flag)
    {
    case VALID:
      {
        *os << "UP";
        break;
      }
    case INVALID:
      {
        *os << "DOWN";
        break;
      }
    case IN_SEARCH:
      {
        *os << "IN_SEARCH";
        break;
      }
    }

  *os << std::setw (16) << expire.str();
  *os << m_hops << std::endl;
  // Restore the previous ostream state
  (*os).copyfmt (oldState);
}

/*
 The Routing Table
 */

RoutingTable::RoutingTable (Time t)
  : m_badLinkLifetime (t)
{
}

bool
RoutingTable::LookupRoute (Ipv6Address id, RoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this << id);
  Purge ();
  if (m_ipv6AddressEntry.empty ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found; m_ipv6AddressEntry is empty");
      return false;
    }
  std::map<Ipv6Address, RoutingTableEntry>::const_iterator i =
    m_ipv6AddressEntry.find (id);
  if (i == m_ipv6AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  rt = i->second;
  NS_LOG_LOGIC ("Route to " << id << " found");
  return true;
}

bool
RoutingTable::LookupValidRoute (Ipv6Address id, RoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this << id);
  if (!LookupRoute (id, rt))
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  NS_LOG_LOGIC ("Route to " << id << " flag is " << ((rt.GetFlag () == VALID) ? "valid" : "not valid"));
  return (rt.GetFlag () == VALID);
}

bool
RoutingTable::DeleteRoute (Ipv6Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  Purge ();
  if (m_ipv6AddressEntry.erase (dst) != 0)
    {
      NS_LOG_LOGIC ("Route deletion to " << dst << " successful");
      return true;
    }
  NS_LOG_LOGIC ("Route deletion to " << dst << " not successful");
  return false;
}

bool
RoutingTable::AddRoute (RoutingTableEntry & rt)
{
  /* std::cout << "\n\nADDING ROUTE"<< std::endl;
  std::cout << "Destination: " << rt.GetDestination() << std::endl;
  std::cout << "Gateway: " << rt.GetRoute()->GetGateway() << std::endl;
  std::cout << "Interface " << rt.GetInterface() << std::endl <<std::endl;*/

  NS_LOG_FUNCTION (this);
  if (rt.GetInterface().GetAddress().IsLinkLocal())
  {
    std::cout << "Destination: " << rt.GetDestination() << std::endl;
    std::cout << "Gateway: " << rt.GetRoute()->GetGateway() << std::endl;
    std::cout << "Interface " << rt.GetInterface() << std::endl <<std::endl;
  }
  /*if (rt.GetInterface().GetAddress().IsLinkLocal())
  {
    std::cout << "not adding route for dst " << rt.GetDestination() << " becuase of interface " <<  rt.GetInterface().GetAddress() << std::endl;
    return false;
  }*/

  Purge ();
  if (rt.GetFlag () != IN_SEARCH)
    {
      rt.SetRreqCnt (0);
    }
  
 /* RoutingTableEntry entry;
  if (LookupRoute(rt.GetDestination(), entry))
  {
    if (entry.GetInterface().GetAddress().IsLinkLocal())
    {
      std::cout << "here" << std::endl;
    }
  }*/

  std::pair<std::map<Ipv6Address, RoutingTableEntry>::iterator, bool> result =
    m_ipv6AddressEntry.insert (std::make_pair (rt.GetDestination (), rt));
  return result.second;
}

bool
RoutingTable::Update (RoutingTableEntry & rt)
{
 /* std::cout << "\n\nUPDATING ROUTE"<< std::endl;
  std::cout << "Destination: " << rt.GetDestination() << std::endl;
  std::cout << "Gateway: " << rt.GetRoute()->GetGateway() << std::endl;
  std::cout << "Interface " << rt.GetInterface() << std::endl <<std::endl;*/

  NS_LOG_FUNCTION (this);
  std::map<Ipv6Address, RoutingTableEntry>::iterator i =
    m_ipv6AddressEntry.find (rt.GetDestination ());
  if (i == m_ipv6AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " fails; not found");
      return false;
    }
  i->second = rt;
  if (i->second.GetFlag () != IN_SEARCH)
    {
      NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " set RreqCnt to 0");
      i->second.SetRreqCnt (0);
    }
  return true;
}

bool
RoutingTable::SetEntryState (Ipv6Address id, RouteFlags state)
{
  NS_LOG_FUNCTION (this);
  std::map<Ipv6Address, RoutingTableEntry>::iterator i =
    m_ipv6AddressEntry.find (id);
  if (i == m_ipv6AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route set entry state to " << id << " fails; not found");
      return false;
    }
  i->second.SetFlag (state);   
  i->second.SetRreqCnt (0);
  NS_LOG_LOGIC ("Route set entry state to " << id << ": new state is " << state);
  return true;
}

void
RoutingTable::GetListOfDestinationWithNextHop (Ipv6Address nextHop, std::map<Ipv6Address, uint32_t> & unreachable )
{
  NS_LOG_FUNCTION (this);
  Purge ();
  unreachable.clear ();
  for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i =
         m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); ++i)
    {
      if (i->second.GetNextHop () == nextHop)
        {
          NS_LOG_LOGIC ("Unreachable insert " << i->first << " " << i->second.GetSeqNo ());
          unreachable.insert (std::make_pair (i->first, i->second.GetSeqNo ()));
        }
    }
}

void
RoutingTable::InvalidateRoutesWithDst (const std::map<Ipv6Address, uint32_t> & unreachable)
{
  NS_LOG_FUNCTION (this);
  Purge ();
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i =
         m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); ++i)
    {
      for (std::map<Ipv6Address, uint32_t>::const_iterator j =
             unreachable.begin (); j != unreachable.end (); ++j)
        {
          if ((i->first == j->first) && (i->second.GetFlag () == VALID))
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              i->second.Invalidate (m_badLinkLifetime);
            }
        }
    }
}

void
RoutingTable::DeleteAllRoutesFromInterface (Ipv6InterfaceAddress iface)
{
  NS_LOG_FUNCTION (this);
  if (m_ipv6AddressEntry.empty ())
    {
      return;
    }
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i =
         m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); )
    {
      if (i->second.GetInterface () == iface)
        {
          std::map<Ipv6Address, RoutingTableEntry>::iterator tmp = i;
          ++i;
          m_ipv6AddressEntry.erase (tmp);
        }
      else
        {
          ++i;
        }
    }
}

void
RoutingTable::Purge ()
{
  NS_LOG_FUNCTION (this);
  if (m_ipv6AddressEntry.empty ())
    {
      return;
    }
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i =
         m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); )
    {
      if (i->second.GetLifeTime () < Seconds (0))
        {
          if (i->second.GetFlag () == INVALID)
            {
              std::map<Ipv6Address, RoutingTableEntry>::iterator tmp = i;
              ++i;
              m_ipv6AddressEntry.erase (tmp);
            }
          else if (i->second.GetFlag () == VALID)
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              i->second.Invalidate (m_badLinkLifetime);
              ++i;
            }
          else
            {
              ++i;
            }
        }
      else
        {
          ++i;
        }
    }
}

void
RoutingTable::Purge (std::map<Ipv6Address, RoutingTableEntry> &table) const
{
  NS_LOG_FUNCTION (this);
  if (table.empty ())
    {
      return;
    }
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i =
         table.begin (); i != table.end (); )
    {
      if (i->second.GetLifeTime () < Seconds (0))
        {
          if (i->second.GetFlag () == INVALID)
            {
              std::map<Ipv6Address, RoutingTableEntry>::iterator tmp = i;
              ++i;
              table.erase (tmp);
            }
          else if (i->second.GetFlag () == VALID)
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              i->second.Invalidate (m_badLinkLifetime);
              ++i;
            }
          else
            {
              ++i;
            }
        }
      else
        {
          ++i;
        }
    }
}

bool 
RoutingTable::GetDestInSearchOfAp(RoutingTableEntry& entry)
{
  
  NS_LOG_FUNCTION (this);
  if (m_ipv6AddressEntry.empty ())
    {
      return false;
    }
 // bool foundEntry = false;
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i =
         m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); )
    {
      if (i->second.IsAccessPoint () && i->second.GetFlag() == IN_SEARCH)
        {
          entry = i->second;
          return true;
        }
        i++;
       /*   std::map<Ipv6Address, RoutingTableEntry>::iterator tmp = i;
          ++i;
          m_ipv6AddressEntry.erase (tmp);
        }
      else
        {
          ++i;
        }*/
    }
  
  return false;
}

bool 
RoutingTable::ActiveApEntries(RoutingTableEntry& entry)
{
  
  NS_LOG_FUNCTION (this);
  if (m_ipv6AddressEntry.empty ())
    {
      return false;
    }
 // bool foundEntry = false;
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i =
         m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); )
    {
      std::cout << "here" << std::endl;
      if (i->second.IsAccessPoint () && i->second.GetFlag() == VALID)
        {
          Ptr<Ipv6Route> route = i->second.GetRoute();
          std::cout << "route\ndst: " << route->GetDestination() << "\nsource: " << route->GetSource() << "\ngateway: " << route->GetGateway() << std::endl;
          entry.SetRoute(i->second.GetRoute());

          return true;
        }
        i++;
       /*   std::map<Ipv6Address, RoutingTableEntry>::iterator tmp = i;
          ++i;
          m_ipv6AddressEntry.erase (tmp);
        }
      else
        {
          ++i;
        }*/
    }
  
  return false;
}

bool
RoutingTable::MarkLinkAsUnidirectional (Ipv6Address neighbor, Time blacklistTimeout)
{
  NS_LOG_FUNCTION (this << neighbor << blacklistTimeout.As (Time::S));
  std::map<Ipv6Address, RoutingTableEntry>::iterator i =
    m_ipv6AddressEntry.find (neighbor);
  if (i == m_ipv6AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Mark link unidirectional to  " << neighbor << " fails; not found");
      return false;
    }
  i->second.SetUnidirectional (true);
  i->second.SetBlacklistTimeout (blacklistTimeout);
  i->second.SetRreqCnt (0);
  NS_LOG_LOGIC ("Set link to " << neighbor << " to unidirectional");
  return true;
}

void
RoutingTable::Print (Ptr<OutputStreamWrapper> stream, Time::Unit unit /* = Time::S */) const
{
  std::map<Ipv6Address, RoutingTableEntry> table = m_ipv6AddressEntry;
  Purge (table);
  std::ostream* os = stream->GetStream ();
  // Copy the current ostream state
  std::ios oldState (nullptr);
  oldState.copyfmt (*os);

  *os << std::resetiosflags (std::ios::adjustfield) << std::setiosflags (std::ios::left);
  *os << "\nAODV Routing table\n";
  *os << std::setw (16) << "Destination";
  *os << std::setw (16) << "Gateway";
  *os << std::setw (16) << "Interface";
  *os << std::setw (16) << "Flag";
  *os << std::setw (16) << "Expire";
  *os << "Hops" << std::endl;
  for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i =
         table.begin (); i != table.end (); ++i)
    {
      i->second.Print (stream, unit);
    }
  *stream->GetStream () << "\n";
}

}
}
