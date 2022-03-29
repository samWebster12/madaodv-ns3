#include <iostream>
#include <cmath>
//#include "ns3/aodv-module.h"
#include "ns3/madaodv-dpd.h"
#include "ns3/madaodv-id-cache.h"
#include "ns3/madaodv-neighbor.h"
#include "ns3/madaodv-packet.h"
#include "ns3/madaodv-routing-protocol.h"
#include "ns3/madaodv-rqueue.h"
#include "ns3/madaodv-rtable.h"
#include "ns3/madaodv-helper.h"
#include "ns3/madaodv-routing-protocol.h"


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping6-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-model.h"
using namespace ns3;

const int size = 9;
const int step = 50;

int main(int argc, char **argv)
{
  //LogComponentEnable("WifiNetDevice", LOG_LEVEL_INFO);
 // LogComponentEnable("StaWifiMac", LOG_LEVEL_INFO);
 // LogComponentEnable("ApWifiMac", LOG_LEVEL_INFO);
 // LogComponentEnable("HybridWifiMac", LOG_LEVEL_INFO);
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
  NodeContainer nonwifinodes;
  NodeContainer wifinodes;
  
  NetDeviceContainer nonwifidevices;
  NetDeviceContainer wifidevices;

 // std::cout << "Creating " << 10 << " nodes " << 10 << " m apart.\n";
  nonwifinodes.Create (size);
  wifinodes.Create(1);

  // Name nonwifinodes
 /* for (uint32_t i = 0; i < size; ++i)
    {
      std::ostringstream os
      os << "node-" << i;
      Names::Add (os.str (), nonwifinodes.Get (i));
    }*/
  // Create static grid
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (step),
                                 "DeltaY", DoubleValue (0),
                                 "GridWidth", UintegerValue (size+1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nonwifinodes);
  mobility.Install (wifinodes);

//  nonwifinodes.Add(wifinodes.Get(0));


  //SETUP MAC LAYER
  WifiMacHelper wifiMac;
 
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "RtsCtsThreshold", UintegerValue (0));

  wifiMac.SetType ("ns3::AdhocWifiMac");
  wifiPhy.SetChannel (wifiChannel.Create ());
  nonwifidevices = wifi.Install (wifiPhy, wifiMac, nonwifinodes);

  wifiMac.SetType ("ns3::ApWifiMac");
  wifidevices = wifi.Install (wifiPhy, wifiMac, wifinodes); 

  
  //SETUP INTERNET STACK
  NodeContainer allNodes;
  allNodes.Add (nonwifinodes);
  allNodes.Add (wifinodes);

  NetDeviceContainer allDevices;
  allDevices.Add (nonwifidevices);
  allDevices.Add (wifidevices);

  MadaodvHelper madaodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (madaodv);

  stack.Install (allNodes);
  Ipv6AddressHelper address (Ipv6Address("100::"), Ipv6Prefix (48));

  Ipv6InterfaceContainer interfaces;
  interfaces = address.Assign (allDevices);


  uint8_t j=0;
  for (Ipv6InterfaceContainer::Iterator i = interfaces.Begin(); i != interfaces.End(); i++)
  {
    i->first->SetForwarding(i->second, true);
   // Ipv6Address addr = GetCorrectIpv6Address(j);
   // Ipv6InterfaceAddress ifaceAddr (addr);
  //  i->first->AddAddress(i->second, ifaceAddr);
    j++;
  }

   Ping6Helper ping; //interfaces.GetAddress (size - 1)
  //ping.SetAttribute ("Verbose", BooleanValue (true));
  std::cout << "Target Address: " << interfaces.GetAddress (size - 1, 1) << std::endl << std::endl;
 // ping.SetRemote(interfaces.GetAddress (size - 1, 1));
  ping.SetRemote(Ipv6Address("fd34:1b20:6cd5:54b1::9")); //fd34:1b20:6cd5:54b1::9
  ping.SetAttribute("Interval", StringValue("30s"));
  ApplicationContainer p = ping.Install (allNodes.Get (0));

  p.Start (Seconds (0));
  p.Stop (Seconds (30) - Seconds (0.001));







  std::cout << "Starting simulation for " << 30 << " s ...\n";

  Simulator::Stop (Seconds (30));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
