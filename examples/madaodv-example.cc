/*
  Example of sending packet to another node in the network
  (node 0 sends to node 8)
  0 RREQ --> 1 --> 2 --> 3 --> 4 --> 5 --> 6 --> 7 --> 8
  0 <-- 1 <-- 2 <-- 3 <-- 4 <-- 5 <-- 6 <-- 7 <-- RREP 8 

  Author: Samuel Webster
 */




#include <iostream>
#include <cmath>

#include "ns3/madaodv-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ping6-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-model.h"
using namespace ns3;
/**
 * \ingroup madaodv-examples
 * \ingroup examples
 * \brief Test script.
 * 
 * This script creates 1-dimensional grid topology and then ping last node from the first one:
 * 
 * [100::1] <-- step --> [100::2] <-- step --> [100::3] <-- step --> [100::4] <-- step --> [100::5] <-- step --> [100::6] <-- step --> [100::7] <-- step --> [100::8] <-- step --> [100::9]
 * 
 * ping 100::9
*/
class MadaodvExample 
{
public:
  MadaodvExample ();
  /**
   * \brief Configure script parameters
   * \param argc is the command line argument count
   * \param argv is the command line arguments
   * \return true on successful configuration
  */
  bool Configure (int argc, char **argv);
  /// Run simulation
  void Run ();
  /**
   * Report results
   * \param os the output stream
   */
  void Report (std::ostream & os);
private:
  // parameters
  /// Number of nodes
  uint32_t size;
  /// Distance between nodes, meters
  double step;
  /// Simulation time, seconds
  double totalTime;
  /// Write per-device PCAP traces if true
  bool pcap;
  /// Print routes if true
  bool printRoutes;
  // network
  /// nodes used in the example
  NodeContainer nodes;
  /// devices used in the example
  NetDeviceContainer devices;
  /// interfaces used in the example
  Ipv6InterfaceContainer interfaces;
private:
  /// Create the nodes
  void CreateNodes ();
  /// Create the devices
  void CreateDevices ();
  /// Create the network
  void InstallInternetStack ();
  /// Create the simulation applications
  void InstallApplications ();

  void LetsSend ();
};

int main (int argc, char **argv)
{
  MadaodvExample test;
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable("MadaodvRoutingProtocol", LOG_LEVEL_INFO);
  
  if (!test.Configure (argc, argv))
    NS_FATAL_ERROR ("Configuration failed. Aborted.");
  test.Run ();
  test.Report (std::cout);
  return 0;
}
//-----------------------------------------------------------------------------
MadaodvExample::MadaodvExample () :
  size (10),
  step (50),
  totalTime (30),
  pcap (true),
  printRoutes (true)
{
}
bool
MadaodvExample::Configure (int argc, char **argv)
{
  // Enable AODV logs by default. Comment this if too noisy
  // LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);
  SeedManager::SetSeed (12345);
  CommandLine cmd (__FILE__);
  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("printRoutes", "Print routing table dumps.", printRoutes);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("step", "Grid step, m", step);
  cmd.Parse (argc, argv);
  return true;
}
void
MadaodvExample::Run ()
{
//  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1)); // enable rts cts all the time.
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();
  std::cout << "Starting simulation for " << totalTime << " s ...\n";
  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
}
void
MadaodvExample::Report (std::ostream &)
{ 
}
void
MadaodvExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
  nodes.Create (size);
  // Name nodes
  for (uint32_t i = 0; i < size; ++i)
    {
      std::ostringstream os;
      os << "node-" << i;
      Names::Add (os.str (), nodes.Get (i));
    }
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
  mobility.Install (nodes);
}
void
MadaodvExample::CreateDevices ()
{
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"), "RtsCtsThreshold", UintegerValue (0));
  devices = wifi.Install (wifiPhy, wifiMac, nodes); 
  if (pcap)
    {
      wifiPhy.EnablePcapAll (std::string ("madaodv"));
    }
}
void
MadaodvExample::InstallInternetStack ()
{
  MadaodvHelper madaodv;
  // you can configure AODV attributes here using aodv.Set(name, value)
  InternetStackHelper stack;
  stack.SetRoutingHelper (madaodv); // has effect on the next Install ()
  stack.Install (nodes);
  Ipv6AddressHelper address;
 // address.SetBase (Ipv6Address("fdf0:7afc:7273:42d5::"), Ipv6Prefix("ffff:ffff:ffff:ffff::"));
  interfaces = address.Assign (devices);
  for (Ipv6InterfaceContainer::Iterator i = interfaces.Begin(); i != interfaces.End(); i++)
  {
    i->first->SetForwarding(i->second, true);
  }


  //Print out all addresses on node.
 /* for (uint32_t i = 0; i < nodes.GetN(); i++) {
    std::cout << std::endl;
    Ptr<Node> node = nodes.Get(i);
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
    for (uint32_t j = 0; j < ipv6->GetNInterfaces(); j++) {
      for (uint32_t k = 0; k < ipv6->GetNAddresses(j); k++) {
        std::cout << "Address on Node " << i << ": " << ipv6->GetAddress(j, k) << std::endl;
      }
      std::cout << "next interface" << std::endl;
    }
    std::cout << std::endl;
  }
  //Print all devices
  
  for (uint32_t i = 0; i < nodes.GetN(); i++) {
    std::cout << std::endl;
    Ptr<Node> node = nodes.Get(i);
    
    for (uint32_t j = 0; j < node->GetNDevices(); j++) {
      Ptr<NetDevice> dev = node->GetDevice(j);
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
      uint32_t iface = ipv6->GetInterfaceForDevice(dev);
      std::cout << "Device " << j << ": " << dev << "\t\t" << ipv6->GetAddress(iface, 0) << std::endl;
    }
    std::cout << std::endl;
  }
  //Print Wifi Devices
  for (uint32_t i = 0; i < devices.GetN(); i++) {
    std::cout << "Wifi Device " << i << ": " << devices.Get(i) << std::endl;
  }*/
  //Print Positions
 /* for (uint32_t i = 0; i < nodes.GetN(); i++) {
    std::vector<
    MobilityModel mob = nodes.Get(i)->GetObject<MobilityModel> ();
    std::cout << mob.GetPositionWithReference()
  }*/
  if (printRoutes)
    {
      Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("madaodv.routes", std::ios::out); // ?
      madaodv.PrintRoutingTableAllAt (Seconds (8), routingStream);
    }
}

void 
MadaodvExample::LetsSend ()
{
  Ptr<Node> node = nodes.Get(3);
  Ptr<Ipv6L3Protocol> ipv6 = node->GetObject<Ipv6L3Protocol> ();

  Ptr<Packet> packet = Create<Packet> ();
  UdpHeader hdr;
  packet->AddHeader (hdr);;

  Ipv6Address from ("2001:db8::200:ff:fe00:4");
  Ipv6Address to ("2001:db8::200:ff:fe00:5");
  Ipv6Route rt;
  rt.SetDestination(to);
  rt.SetSource(from);
  rt.SetGateway(to);
  rt.SetOutputDevice(devices.Get(3));

  Ptr<Ipv6Route> ptr (&rt);

  std::cout << "\n\nsletssend packet\n\n" << std::endl;
  ipv6->Send(packet, from, to, 17, ptr);
}

void
MadaodvExample::InstallApplications ()
{

  Ping6Helper ping; //interfaces.GetAddress (size - 1)
  //ping.SetAttribute ("Verbose", BooleanValue (true));
  std::cout << "Target Address: " << interfaces.GetAddress (size - 1, 1) << std::endl << std::endl;
  ping.SetRemote(interfaces.GetAddress (size - 1, 1));

  ApplicationContainer p = ping.Install (nodes.Get (0));

  p.Start (Seconds (0));
  p.Stop (Seconds (totalTime) - Seconds (0.001));

 // ipv6->Send(packet, from, to, 17, ptr);
 // Simulator::Schedule (Seconds (1.5), &MadaodvExample::LetsSend, this);
  // move node away
 // Ptr<Node> node = nodes.Get (size/2);
  //Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
  //Simulator::Schedule (Seconds (totalTime/3), &MobilityModel::SetPosition, mob, Vector (1e5, 1e5, 1e5));
}









