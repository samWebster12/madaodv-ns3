# madaodv
**(in development)**
## Table Of Contents
## Description
The goal of the madaodv protocol is to allow nodes without access to the internet, to access the internet through nodes that do have access to the internet. The protocol creates routes between a set of nodes to connect nodes without internet access to nodes that do have access, so as to allow communication between them. From there, using a higher layer protoocol, nodes that provide internet access can act as gateways that pass information to the internet from nodes without access to the internet and vice versa. 

In order to accomplish this goal, madaodv builds off the already existing protocol, aodv, using the same control packets, RREQ, RREP, and RERR to establish routes. The key change to madaodv is in including a flag within the RREQ that allows nodes to target any destination with access to the internet, rather than a single, definite address. For more information on AODV, [click here](https://datatracker.ietf.org/doc/html/rfc3561)

Since nodes that are part of the madaodv netowrk are not assumed to have a legitimate internet address, a different method for addressing nodes is needed. To solve this problem, at initilization, madaodv assigns nodes an ipv6 address(es) from a pre-defined range with a device mac address embeded within. Currently that range is 100::/48, with the last 48 bits being a device mac address. 



## How To Install and Run

## Hybrid Wifi Mac

## Future Plans

