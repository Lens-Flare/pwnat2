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
#define PK_ADVERTIZE	1
#define PK_SERVICE		2
#define PK_REQUEST		3
#define PK_FORWARD		4

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
	uint8_t data[]; // bytes
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

// a service info request packet - PK_REQUEST
struct pk_request {
	struct pk_keepalive _super;
};

typedef struct pk_keepalive pk_keepalive_t;
typedef struct pk_advertize pk_advertize_t;
typedef struct pk_service pk_service_t;
typedef struct pk_request pk_request_t;

#pragma pack(pop)

void hton_pk(pk_keepalive_t * pk);
void ntoh_pk(pk_keepalive_t * pk);

#endif
