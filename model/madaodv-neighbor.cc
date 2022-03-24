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

#include <algorithm>
#include "ns3/log.h"
#include "ns3/wifi-mac-header.h"
#include "madaodv-neighbor.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MadaodvNeighbors");

namespace madaodv {
  
Neighbors::Neighbors (Time delay)
  : m_ntimer (Timer::CANCEL_ON_DESTROY)
{
  m_ntimer.SetDelay (delay);
  m_ntimer.SetFunction (&Neighbors::Purge, this);
  m_txErrorCallback = MakeCallback (&Neighbors::ProcessTxError, this);
}

bool
Neighbors::IsNeighbor (Ipv6Address addr)
{
  Purge ();
  for (std::vector<Neighbor>::const_iterator i = m_nb.begin ();
       i != m_nb.end (); ++i)
    {
      if (i->m_neighborAddress == addr)
        {
          return true;
        }
    }
  return false;
}

Time
Neighbors::GetExpireTime (Ipv6Address addr)
{
  Purge ();
  for (std::vector<Neighbor>::const_iterator i = m_nb.begin (); i
       != m_nb.end (); ++i)
    {
      if (i->m_neighborAddress == addr)
        {
          return (i->m_expireTime - Simulator::Now ());
        }
    }
  return Seconds (0);
}

void
Neighbors::Update (Ipv6Address addr, Time expire)
{
  for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
    {
      if (i->m_neighborAddress == addr)
        {
          i->m_expireTime
            = std::max (expire + Simulator::Now (), i->m_expireTime);
          if (i->m_hardwareAddress == Mac48Address ())
            {
              i->m_hardwareAddress = LookupMacAddress (i->m_neighborAddress);
            }
          return;
        }
    }

  NS_LOG_LOGIC ("Open link to " << addr);
  Neighbor neighbor (addr, LookupMacAddress (addr), expire + Simulator::Now ());
  m_nb.push_back (neighbor);
  Purge ();
}

void
Neighbors::AddNdiscCache (Ptr<NdiscCache> a)
{
  m_ndisc.push_back (a);
}

void
Neighbors::DelNdiscCache (Ptr<NdiscCache> a)
{
  m_ndisc.erase (std::remove (m_ndisc.begin (), m_ndisc.end (), a), m_ndisc.end ());
}


/**
 * \brief CloseNeighbor structure
 */
struct CloseNeighbor
{
  /**
   * Check if the entry is expired
   *
   * \param nb Neighbors::Neighbor entry
   * \return true if expired, false otherwise
   */
  bool operator() (const Neighbors::Neighbor & nb) const
  {
    return ((nb.m_expireTime < Simulator::Now ()) || nb.close);
  }
};

void
Neighbors::Purge ()
{
  if (m_nb.empty ())
    {
      return;
    }

  CloseNeighbor pred;
  if (!m_handleLinkFailure.IsNull ())
    {
      for (std::vector<Neighbor>::iterator j = m_nb.begin (); j != m_nb.end (); ++j)
        {
          if (pred (*j))
            {
              NS_LOG_LOGIC ("Close link to " << j->m_neighborAddress);
              m_handleLinkFailure (j->m_neighborAddress);
            }
        }
    }
  m_nb.erase (std::remove_if (m_nb.begin (), m_nb.end (), pred), m_nb.end ());
  m_ntimer.Cancel ();
  m_ntimer.Schedule ();
}

void
Neighbors::ScheduleTimer ()
{
  m_ntimer.Cancel ();
  m_ntimer.Schedule ();
}


Mac48Address
Neighbors::LookupMacAddress (Ipv6Address addr)
{
  Mac48Address hwaddr;
  uint8_t ipv6Buffer[16];
  addr.GetBytes(ipv6Buffer);

  uint8_t macBuffer[6];

  // We want bytes 11-16 (where the stars in: 100:0:0:0:0:*:*:*)
  for (uint8_t i = 0; i < 6; i++)
  {
    macBuffer[(int)i] = ipv6Buffer[(int)i+10];  
  }

  hwaddr.CopyFrom((const uint8_t*) macBuffer);

  return hwaddr;
}

void
Neighbors::ProcessTxError (WifiMacHeader const & hdr)
{
  Mac48Address addr = hdr.GetAddr1 ();

  for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
    {
      if (i->m_hardwareAddress == addr)
        {
          i->close = true;
        }
    }
  Purge ();
}

}  // namespace madaodv
}  // namespace ns3

