### viamillipede:
Fast, resiliant, network transparent pipe tranport. 
![alt text](newetop.svg "parallelize traffic")

Viamillipede is client/server program built to improve pipe transport across networks by using multiple TCP sessions. It demultiplexes stdin into multiple buffered TCP connectons and then terminates the connections into stdout on another host. Order is guaranteed and the pipe is transparent to the source/sink programs. It is as simple to use as Netcat and can generate large throughputs.

#### Problems With existing approaches:
TCP connections are fragile and IP employs best effort delivery to preserve its economy.  TCP was not engineered for performance, resiliance or longevity for a single flow.  Relying on a single TCP connection to succeed or perform is not defendable in software however there are many applications where losing a TCP session is expensive.

### typical pathology:
![alt text](series_is_trouble.svg "serialized operations show resistance")
 + ```tar cf - / | /bin/dd obs=1m 2> /dev/null | /bin/dd obs=1m 2> /dev/null | /usr/local/bin/pipewatcher | ssh dest.yoyo.com "tar xf- "```
 + double serial penalty slow throughput, note latent mistake causing 1B reads
 + Extra pipe steps impose 'serial resistance' to throughput.
 + desperately superstitious optimizations are premature and not generally applicable
 + ssh is not the tool for every situation 
 + a fixed pipeline is not tuned for any system
 + SMP ignorance, Cpu's either lonely forever or hotspotted.
 + underused tx/rx interrupt endpoints, pcie lanes, NIC channel workers, memory lanes and flow control.
 + networks are tuned against hot single tcp connections; that is hard to fix
 + poor mss window scaling. Congestion controls aggressively collapse when network conditions are not pristine.
 + large bandwidth latency product vs. contended lans; both penalized due to 'impedence mismatches') 
 + Poor buffer interactions eg: "Shoe shining" delays. 
 + NewReno congestion control alternatives are not always practical.
 + Flows are stuck on one L1/L2/L3 path.  This defeats the benefits of aggregation and multi-homed connections.
 + Alternate parallel transports are not pipe transparent and require significant configuration; eg: pftp, bittorrent, pNFS, ppcp
 + Your NOC will do maintenance in intervals shorter than your data migration windows. eg: I need to move a petabyte over the wan, but the router is booted every week. 

#### Goals and Features of viamillipede:
![alt text](parallel.svg "parallel")
+ Provide:
     + Fast transparent delivery of long lived pipes across typical networks.
     + Runtime SIGINFO inspection of traffic flow.`( parallelism, worker allocation, total throughput )`
     + Resilience against dropped TCP connections and dead links.
+ Increase traffic throughput by:
	+ Using parallel connections that each vie for survival against adverse network conditions.
	+ Using multiple destination addresses with LACP/LAGG or separate Layer 2 adressing.
	+ Permit adequate buffering to prevent shoe shining. 
	+ Return traffic is limited to ACK's to indicate correct operations
+ Specified Traffic Shaping:
     + Steer traffic to preferred interfaces.
     + Use multiple physical or logical IP transports.
     + Use aggregated link throughput in a user specified sorted order.

+ Error resiliance: TCP sessions are delicate things
     + Restart broken TCP sessions on alternate transport automatically. 
     + Bypass dead links at startup; retry them later as other network topology changes are detected.
     + self tuning worker count, side chain, link choices and buffer sizes, Genetic optimization topic? `(*)`
     + checksums, not needed, but it's part of the test suite, use the 'checksums' transmitter flag to activate
     + error injection via tx chaos <seed> option - break the software in weird ways,  mostly for the test suite
     + programmable checkphrase  uses a 4 character checkphrase to avoid confusion rather than provide strong authenticaion

+ Simple to use in pipe filter programs
     + Why hasn't someone done this before? 
     + IBM's parallel ftp for SP2 z/os, bittorrent: not pipe transparent. 
     + Hide complexity of parallel programming model from users.

#### Examples:

### Simple operation:
+ Start receiver with rx <portnum> and provide a stdout destination 
+``` viamillipede rx 8834 > /tmp/despair  ```
+ Start transmitter with  tx <receiver_host> <portnum> and provide a stdin source
+ ``` cat /tmp/Osymandias  | viamillipede tx host1.yoyodyne.com 8834  ```
	     
### Use case with zfs send/recv:
+ Start transmitter with  tx <receiver_host> <portnum>  and provide stdin from zfs send	
+ ``` zfs send dozer/visage | viamillipede tx foriegn.shore.net 8834  ```
+ Start receiver  with rx <portnum>  and ppipe output to zfs recv
+ ``` viamillipede rx 8834   | zfs recv trinity/destset ```
	
### Options:
+ `rx <portnum> ` Become a reciever. Write output to stdout. 
+ `tx <host> <portnum> ` Become a transmitter and add transport graph link toward an rx host.  Optionally provide tx muliple times to inform us about transport alternatives. We fill tcp queues on the first entries and then proceed down the list if there is more input than link throughput.  It can be helpful to provide multiple ip aliases to push work to different nic channel workers and balance traffic across LACP hash lanes. Analysis of the network resources shold inform this graph. You may use multiple physical interfaces by chosing rx host ip's that force multiple routes.
	+ Read stdin and push it over the network. 
	+ Full duplex, rx and tx may be used simultaneously to provide a transparent full duplex pipe. Happy shell throwing!
		+ Two disinct port numbers are requied, one rx port for each side, with the tx on the other host pointing at the rx
		+ host1: ./vimaillipede rx  7788 tx host2 9900 charmode
		+ host2: ./vimaillipede rx  9900 tx host1 7788 charmode
	+ The source and destination machine may have multiple interfaces and may have:
		+ varying layer1 media ( ethernet, serial, Infiniband , 1488, Carrier Pidgeon, insects, ntb)
		+ varying layer2 attachment ( vlan, aggregation )
		+ varying layer3 routes ( multihomed transport )
	+ Use the preferred link.  Should you saturate it,  fill the next available link.
	+ Provide tx multiple times to describe the transport graph.
	+ Provide tx the same number of times as the thread count to precisely distribute traffic on specific links
	+ Provide prbs 0x5a5a  generate or verify pseudorandom bit stream for load testing 
``` viamillipede \
	tx host1.40g-infiniband.yoyodyne.com\
	tx host1a.40g-infiniband.yoyodyne.com\
	tx host1.internal10g.yoyodyne.com\
	tx host1.internal1g.yoyodyne.com\
	tx host1.expensive-last-resort.yoyodyne.com

```

+ terminate <port>
	+ transmitter or receiver, requires full duplex
	+ Accept a tcp connection
+ initiate <hostname> <port>
	+ transmitter or receiver, requires full duplex
	+ Create a tcp socket
	+ use with terminate to tunnel a full duplex socket, this example tunnels ssh from host1:9022 to host2:22
```
		host1: ./vimaillipede rx  7788 tx host2 9900 charmode terminate 9022
		host2: ./vimaillipede rx  9900 tx host1 7788 charmode initiate localhost 22
```

+ charmode
	+ transmitter or receiver
	+ Attempt to deliver any data in the buffer, do not wait for buffers to fill strictly
+ verbose  <0-20+>, 
	+ transmitter or receiver
	+ ``` viamillipede rx 8834   verbose 5 ```
+ threads <1-16> control worker thread count 
	+ set only on transmitter
	+ tune this value to suit your need
	+ An upper limit of 16 is statically compiled in, higher thread count is unlikely to be productive. 
	+ A minimum of 3 threads is encoraged to preserve performance and resiliancy. 
	+ ``` viamillipede tx foreign.shore.net 8834 threads 16 ```
+ checksum (only on transmitter). 
	+ This is probably not required as the tcp stack and network layer will perform this autmatically
	+ part of the verification suite or for the paranoid user
	+ uses a fast, not particularly stellar, method
	+ transmitter only option.
	+ ``` viamillipede tx foreign.shore.net 8834 checksum ```
+ chaos <clock divider> add error via chaos
	+ transmitter only option
	+ could be used to rebalance network links
	+ periodically close sockets to simulate real work network trouble  and tickle recovery code
	+ deterministic for how many operations to allow before a failure
	+ ``` viamillipede tx localhost 12334 chaos 1731```
+ checkphrase <char[4]> provide lightweight guard agaist a stale or orphaned reciever,
	+ not a security/authenticatio mechanism
	+ Transmitter and Reciever word[4] must match exactly.
	+ ``` viamillipede tx localhost 12334 checkphrase wat!```
	+ ``` viamillipede rx 12334 checkphrase wat!```


### Use outboard crypto: viamillipede does not provide any armoring against interception or authentication
+ use ipsec/vpn and live with the speed
+ provide ssh tcp forwarding endpoints
	+ from the tx host:` ssh -N -L 12323:localhost:12323 tunneluser@rxhost `
	+ use mutiple port instances to  get parallelism
	* use a trusted peers tcp encapuslation tunnel to offload crypto
+ use openssl in  stream, take your crypto into your own hands
	+ ` /usr/bin/openssl enc -aes-128-cbc -k swordfIIIsh -S 5A  `
	+ choose a cipher that's binary transparent  
	+ appropriate  paranoia vs. performance up to you
	+ enigma, rot39, morse?
### Theory of operation
![alt text](theory_operation_viamillipede.svg "theory of operation")
### Future plans  `(*)` 
	+  work in progress, because "hard * ugly > time"
	+ Make additional 'sidechain' compression/crypto pipeline steps parallel.
     		+ hard due to unpredictable buffer size dynamics
        	+ sidechains could include any reversable pipe transparent program
        	+ gzip, bzip2
        	+ openssl
        	+ rot39, od
	+ xdr/rpc marshalling for architecture independence
 		+ serializing a struct is not ideal
	+ reverse channel capablity  *done 20180830
		+ millipedesh ? millipederpc?
		+ specify rx/tx at the same time + fifo?
		+ is this even a good idea? Exploit generator?
		+ provide proxy trasport for other bulk movers: 
			+ rsync 
			+ ssh 
			+ OpenVPN
		+ error feedback path more that ack
		+ just run two tx/rx pairs?

