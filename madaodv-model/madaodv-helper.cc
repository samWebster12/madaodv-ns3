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
 * Authors: Pavel Boyko <boyko@iitp.ru>, written after OlsrHelper by Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "madaodv-helper.h"
//#include "ns3/madaodv-routing-protocol.h"
#include "madaodv-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"
#include "ns3/ipv6-list-routing.h"

namespace ns3
{

MadaodvHelper::MadaodvHelper() : 
  Ipv6RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::madaodv::RoutingProtocol");
}

MadaodvHelper* 
MadaodvHelper::Copy (void) const 
{
  return new MadaodvHelper (*this); 
}

Ptr<Ipv6RoutingProtocol> 
MadaodvHelper::Create (Ptr<Node> node) const
{
  Ptr<madaodv::RoutingProtocol> agent = m_agentFactory.Create<madaodv::RoutingProtocol> ();
  node->AggregateObject (agent);
  return agent;
}

void 
MadaodvHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

int64_t
MadaodvHelper::AssignStreams (NodeContainer c, int64_t stream)
{
  int64_t currentStream = stream;
  Ptr<Node> node;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      node = (*i);
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
      NS_ASSERT_MSG (ipv6, "Ipv6 not installed on node");
      Ptr<Ipv6RoutingProtocol> proto = ipv6->GetRoutingProtocol ();
      NS_ASSERT_MSG (proto, "Ipv6 routing not installed on node");
      Ptr<madaodv::RoutingProtocol> madaodv = DynamicCast<madaodv::RoutingProtocol> (proto);
      if (madaodv)
        {
          currentStream += madaodv->AssignStreams (currentStream);
          continue;
        }
      // madAodv may also be in a list
      Ptr<Ipv6ListRouting> list = DynamicCast<Ipv6ListRouting> (proto);
      if (list)
        {
          int16_t priority;
          Ptr<Ipv6RoutingProtocol> listProto;
          Ptr<madaodv::RoutingProtocol> listMadaodv;
          for (uint32_t i = 0; i < list->GetNRoutingProtocols (); i++)
            {
              listProto = list->GetRoutingProtocol (i, priority);
              listMadaodv = DynamicCast<madaodv::RoutingProtocol> (listProto);
              if (listMadaodv)
                {
                  currentStream += listMadaodv->AssignStreams (currentStream);
                  break;
                }
            }
        }
    }
  return (currentStream - stream);
}

}
