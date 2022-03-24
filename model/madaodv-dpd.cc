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
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */

#include "madaodv-dpd.h"

namespace ns3 {
namespace madaodv {

bool
DuplicatePacketDetection::IsDuplicate  (Ptr<const Packet> p, const Ipv6Header & header)
{
  return m_idCache.IsDuplicate (header.GetSourceAddress (), p->GetUid () );
}
void
DuplicatePacketDetection::SetLifetime (Time lifetime)
{
  m_idCache.SetLifetime (lifetime);
}

Time
DuplicatePacketDetection::GetLifetime () const
{
  return m_idCache.GetLifeTime ();
}


}
}

