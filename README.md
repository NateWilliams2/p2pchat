# p2pchat
Simple p2p chat application built with C

Written as a project for csc-213 at Grinnell College

## Architecture
The first client to run this application uses 
```
./p2pchat username
```
to establish a socket listening for messages from peers. 

As new peers want to join a group of peers, the must connect to some client which is already running within the group: 
```
./p2pchat username peer-server peer-server-port
```
This connection acts as the new peer's server. When our new peer sends a message, this server forwards it up the tree to all other peers. When some peer further up the tree sends a message, it is forwarded through our peer's server. 
