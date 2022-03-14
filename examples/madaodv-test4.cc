#include <iostream>
#include <cmath>
//#include "ns3/aodv-module.h"
#include "madaodv-dpd.h"
#include "madaodv-id-cache.h"
#include "madaodv-neighbor.h"
#include "madaodv-packet.h"
#include "madaodv-routing-protocol.h"
#include "madaodv-rqueue.h"
#include "madaodv-rtable.h"
#include "madaodv-helper.h"
#include "madaodv-routing-protocol.h"


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping6-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-model.h"
using namespace ns3;

const int size = 4;
const int step = 50;

int main(int argc, char **argv)
{
  //LogComponentEnable("WifiNetDevice", LOG_LEVEL_INFO);
 // LogComponentEnable("StaWifiMac", LOG_LEVEL_INFO);
  LogComponentEnable("ApWifiMac", LOG_LEVEL_ALL);
  LogComponentEnable("HybridWifiMac", LOG_LEVEL_ERROR);
  NodeContainer nonwifinodes;
  NodeContainer wifinodes;
  
  NetDeviceContainer nonwifidevices;
  NetDeviceContainer wifidevices;

 // std::cout << "Creating " << 10 << " nodes " << 10 << " m apart.\n";
  nonwifinodes.Create (size);
  wifinodes.Create(1);

  // Name nonwifinodes
  /*for (uint32_t i = 0; i < 10; ++i)
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
                                 "GridWidth", UintegerValue (size),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nonwifinodes);
  mobility.Install (wifinodes);


  //nonwifinodes
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::HybridWifiMac");
 
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "RtsCtsThreshold", UintegerValue (0));


  nonwifidevices = wifi.Install (wifiPhy, wifiMac, nonwifinodes);

  wifiMac.SetType ("ns3::ApWifiMac");

  wifidevices = wifi.Install (wifiPhy, wifiMac, wifinodes); 



  std::cout << "Starting simulation for " << 30 << " s ...\n";

  Simulator::Stop (Seconds (30));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
