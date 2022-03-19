# MAC-Address Determined Adhoc Ondemand Routing Protocol
## Table Of Contents
The goal of the mad aodv protocol is to allow nodes without access to the internet, to access the internet through nodes that do have access to the internet. The actual protocol creates routes between an indefinite set of nodes to connect nodes with internet access to nodes that do, so as to allow communication between them. In order to accomplish this goal, madaodv builds off the already existing protocol, aodv, using the same control packets, RREQ, RREP, and RERR to establish routes. The key insight of madaodv is including a flag within the RREQ that allows nodes to target any destination with access to the internet, rather than a single, definite address. However, in order for such a protocol to work, it must use definite addresses that are defined at the initialization of a node, which is where mac addresses come into play. 

## Description
## How To Install and Run
