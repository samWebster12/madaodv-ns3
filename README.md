# madaodv
**(in development)**
## Table Of Contents
## Description
The goal of the madaodv protocol is to allow nodes without access to the internet, to access the internet through nodes that do have access to the internet. The protocol creates routes between a set of nodes to connect nodes without internet access to nodes that do have access, so as to allow communication between them. From there, using a higher layer protoocol, nodes that provide internet access can act as gateways that pass information to the internet from nodes without access to the internet and vice versa. 

In order to accomplish this goal, madaodv builds off the already existing protocol, aodv, using the same control packets, RREQ, RREP, and RERR to establish routes. The key change to madaodv is in including a flag within the RREQ that allows nodes to target any destination with access to the internet, rather than a single, definite address. For more information on AODV, [click here](https://datatracker.ietf.org/doc/html/rfc3561)

Since nodes that are part of the madaodv netowrk are not assumed to have a legitimate internet address, a different method for addressing nodes is needed. To solve this problem, at initilization, madaodv assigns nodes an ipv6 address(es) from a pre-defined range with a device mac address embeded within. Currently that range is 100::/48, with the last 48 bits being a device mac address. 



## How To Install and Run
### Prerequisites
To run simulations, you will need to install ns3 version 3.34 <br />
To do so, click this [link](https://www.nsnam.org/releases/ns-allinone-3.34.tar.bz2)


Next, **cd into the downloaded directory** and run the command: <br />

```git clone https://github.com/samWebster12/madaodv-ns3 ns-3.34/src/madaodv```

Next, changes will need to be made to some files. To see a full list of changes, visit the **Supplemental Files** section <br />
Run the commands: </br>
```
rm ns-3.34/src/internet/icmpv6-l4-protocol.cc ns-3.34/src/internet/ipv6-end-point-demux.cc ipv6-interface.cc ipv6-l3-protocol.cc 
cp ns-3.34/src/madaodv/supplemental_files/icmpv6-l4-protocol.cc ns-3.34/src/internet/icmpv6-l4-protocol.cc
cp ns-3.34/src/madaodv/supplemental_files/ipv6-end-point-demux.cc ns-3.34/src/internet/ipv6-end-point-demux.cc
cp ns-3.34/src/madaodv/supplemental_files/ipv6-interface ns-3.34/src/internet/ipv6-interface
cp ns-3.34/src/madaodv/supplemental_files/ipv6-l3-protocol.cc ns-3.34/src/internet/ipv6-l3-protocol.cc
```

### Compiliation
To compile, configure tests and then build ns3: <br />
```
./waf configure --enable-tests --enable-examples <br/>
./waf build
```

## Supplemental Files
These files have modifications from the original files that allow madaodv to work properly. <br/>
The changes are listed below:

### icmpv6-l4-protocol.cc
- Got rid of Icmpv6L4Protocol Handle RS method lines 452 to 458 (everything but first line)

### ipv6-end-point-demux.cc
- added case for all nodes multicast in Ipv6EndPointDemux Lookup method line 232
- added  line: bool localAddressMatchesWildcard = endP->GetLocalAddress () == Ipv6Address::GetAny();
- changed if to : if (!(localAddressMatchesExact || localAddressMatchesWildCard || destAddressIsAllNodesMulticast))
- changed first of bottom ifs to : if ((localAddressMatchesWildCard || destAddressIsAllnodesMulticast) && ...)


### ipv6-interface.cc
- Commented out "Simulator::Schedule (Seconds (0.), &Icmpv6L4Protocol::FunctionDadTimeout, icmpv6, this, addr);" in Ipv6Interface::AddAddress (in else statement)

### ipv6-l3-protocol.cc
- added a new case to ipv6-l3-protocol, Send function (case of allnode multicast):

## Hybrid Wifi Mac
Development of a Hybrid Wifi Mac that provides support for both infrastrucutre mode and adhoc is being workd on in the hybrid-wifi-mac folder. The goal is to allow nodes to connect to wifi infrasture mode and also connect to madaodv network, providing access to the internet to the madaodv network through this wifi infrastructure. 

## Additional Necessary Technologies for Practical Use
In order to be of any true use, madaodv requires that two other technologies are implemented:

First, the ability for a wifi device to connect to an access point (infrastructure mode) and simultaneously connect to the madaodv network (ad hoc mode) is important so that access points may provide internet access at no cost to them (rather than using lte). This is what the hybrid-wifi-mac is designed for.

Second, an application level protocol is needed for two reasons. First, the protocol needs to provide NAT-like functionality, multiplexing and demultiplexing  incoming addresses and ports so gateways may send packets to the internet and back on behalf of other nodes. Second, information such as what URL or address to go to need to be sent to access points in some way. An application level protocol would allow nodes to send information that needs to be sent through the internet via

## Future Plans





