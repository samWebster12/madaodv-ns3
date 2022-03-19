# madaodv
**(in development)**
## Table Of Contents
## Description
The goal of the madaodv protocol is to allow nodes without access to the internet, to access the internet through nodes that do have access to the internet. The protocol creates routes between a set of nodes to connect nodes without internet access to nodes that do have access, so as to allow communication between them. From there, using a higher layer protoocol, nodes that provide internet access can act as gateways that pass information to the internet from nodes without access to the internet and vice versa. 

In order to accomplish this goal, madaodv builds off the already existing protocol, aodv, using the same control packets, RREQ, RREP, and RERR to establish routes. The key change to madaodv is in including a flag within the RREQ that allows nodes to target any destination with access to the internet, rather than a single, definite address. For more information on AODV, [click here](https://datatracker.ietf.org/doc/html/rfc3561)

Since nodes that are part of the madaodv netowrk are not assumed to have a legitimate internet address, a different method for addressing nodes is needed. To solve this problem, at initilization, madaodv assigns nodes an ipv6 address(es) from a pre-defined range with a device mac address embeded within. Currently that range is 100::/48, with the last 48 bits being a device mac address. 



## How To Install and Run

## Hybrid Wifi Mac
Development of a Hybrid Wifi Mac that provides support for both infrastrucutre mode and adhoc is being workd on in the hybrid-wifi-mac folder. The goal is to allow nodes to connect to wifi infrasture mode and also connect to madaodv network, providing access to the internet to the madaodv network through this wifi infrastructure. 

## Additional Necessary Technologies for Practical Use
In order to be of any true use, madaodv requires that two other technologies are implemented:

First, the ability for a wifi device to connect to an access point (infrastructure mode) and simultaneously connect to the madaodv network (ad hoc mode) is important so that access points may provide internet access at no cost to them (rather than using lte). This is what the hybrid-wifi-mac is designed for.

Second, an application level protocol is needed for two reasons. First, the protocol needs to provide NAT-like functionality, multiplexing and demultiplexing  incoming addresses and ports so gateways may send packets to the internet and back on behalf of other nodes. Second, information such as what URL or address to go to need to be sent to access points in some way. An application level protocol would allow nodes to send information that needs to be sent through the internet via

## Future Plans





