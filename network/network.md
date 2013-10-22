Network
=======

pwnat2 uses `packet` structures to send data between its three components, the
server, providers, and consumers. There are a number of types of packets:

  * Keepalive - the most basic type of packet. It has a signature byte which
  must always equal 0xB6, a four byte long software version field (major, minor
  version, revision, subrevision, one byte each), a four byte network version
  (changes when the structures change), a byte for the packet type, and a byte
  for the size of the packet. *The maximum packet size is 255*. **All other
  packet structures include this one**.
  
  * Bad Software Version - the sender received a packet with a software version
  incompatible with its own.
  
  * Bad Network Version - the sender received a packet with a network version
  incompatible with its own.
  
  * Bad Packet - the sender received a packet it shouldn't have.
  
  * Handshake - used in the handshake process. It adds two 32 byte fields, the
  first for data, the second for the SHA256 hash of said data, and a byte for
  the handshake step. *See the Handshake section below*.
  
  * Advertize - used by the provider to advertize a service. It adds two bytes
  for the service's port number, four bytes reserved for future use, and a field
  for the service's name. *See the Other Structures section below*. **The name
  field must be null terminated, unless it is one character long and that
  character is `-` (there is no longer a service on this port) or `+` (find the
  name for the service based on the port number and `/etc/services`)**.
  
  * Request - used by the consumer to request the list of services from the
  server.
  
  * Service - used by the server to inform the consumer the consumer of avaiable
  services. It adds an address field, a two byte field for the port number, a
  four byte field reserved for future use, and a name field. *See the Other
  Structures section below*. **The name field must be null terminated**.
  
  * Response - used by the server to tell the consumer all of the service
  packets have been sent.
  
  * Forward - used to forward captured packets. It has not been defined yet.
  
  * Exiting - used by any element to inform other elements that it is shutting
  down.



Other Structures
----------------

  * Address - this field has a byte for the address family that can either be
  `AF_INET` or `AF_INET6` and a a 16 byte field that contains the IP address
  data. *If the family is `AF_INET`, only the first 4 bytes are used*.
  
  * String - this field starts with a byte for the length of the string,
  including any null termination, and then a variable number of bytes for the
  string's characters.



Handshaking
-----------

The handshake packets (3 in total) must be the first packets sent and received
when a new connection is opened.

The connection initializer generates and sends a handshake packet with a step of
`PK_HS_INITIAL` and random data. The hash field must always be the SHA256 hash
of the data field. The recipient of this packet creates and sends a new packet
with a step of `PK_HS_ACKNOWLEDGE` and data as the received packet's hash. This
process is repeated one last time, with a step of `PK_HS_FINAL`.



API
---

Under progress.