//
//  network.h
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#ifndef pwnat2_network_h
#define pwnat2_network_h

#include <stdint.h>
#include <netdb.h>

#define PK_KEEPALIVE	0
#define PK_BADNETVER	1
#define PK_BADSWVER		2
#define PK_ADVERTIZE	3
#define PK_SERVICE		4
#define PK_REQUEST		5
#define PK_FORWARD		6

#define NET_VER			1

#pragma pack(push)
#pragma pack(1)

// an IP address
struct _pk_address {
	sa_family_t family; // equals AF_INET or AF_INET6
	uint32_t data[4]; // cast to in_addr or in6_addr
};

// a string
struct _pk_string {
	uint32_t length; // number of bytes
	uint8_t data[]; // bytes (null terminated)
};

// a keepalive packet - PK_KEEPALIVE
struct pk_keepalive {
	uint8_t version[4]; // software version
	uint32_t netver; // network structure version
	uint8_t type; // packet type
	uint32_t size; // packet size
};

// a service advertizing packet - PK_ADVERTIZE
struct pk_advertize {
	struct pk_keepalive _super;
	uint16_t port; // port number
	uint32_t reserved; // reserved - for future use
	struct _pk_string name; // service name
};

// a service info response packet - PK_SERVICE
struct pk_service {
	struct pk_keepalive _super;
	struct _pk_address address; // host address
	uint16_t port; // port number
	uint32_t reserved; // reserved - for future use
	struct _pk_string name; // service name
};

typedef struct pk_keepalive pk_keepalive_t;
typedef struct pk_advertize pk_advertize_t;
typedef struct pk_service pk_service_t;

#pragma pack(pop)

void hton_pk(pk_keepalive_t * pk);
void ntoh_pk(pk_keepalive_t * pk);

int open_socket(char * hostname, char * servname, int * sockfd);

pk_keepalive_t * alloc_packet(unsigned long size);
void init_packet(pk_keepalive_t * pk, unsigned char type);
void free_packet(pk_keepalive_t * pk);

pk_keepalive_t * make_pk_keepalive(unsigned char type);
pk_advertize_t * make_pk_advertize(unsigned short port, const char * name);
pk_service_t * make_pk_service(struct in_addr address, unsigned short port, const char * name);
pk_service_t * make_pk_service6(struct in6_addr address, unsigned short port, const char * name);

#endif
