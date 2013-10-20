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

#define NET_VER			1

enum pk_type {
	PK_KEEPALIVE,
	PK_BADSWVER,
	PK_BADNETVER,
	PK_ADVERTIZE,
	PK_REQUEST,
	PK_RESPONSE,
	PK_SERVICE,
	PK_FORWARD
};

#pragma pack(push)
#pragma pack(1)

// an IP address
struct _pk_address {
	uint8_t family; // equals AF_INET or AF_INET6
	uint32_t data[4]; // cast to in_addr or in6_addr
};

// a string
struct _pk_string {
	uint8_t length; // number of bytes
	uint8_t data[]; // bytes (null terminated)
};

// a keepalive packet - PK_KEEPALIVE
struct pk_keepalive {
	uint8_t version[4]; // software version
	uint32_t netver; // network structure version
	uint8_t type; // packet type
	uint8_t size; // packet size
};

// a service advertizing packet - PK_ADVERTIZE
struct pk_advertize {
	struct pk_keepalive _super;
	uint16_t port; // port number
	uint32_t reserved; // reserved - for future use
	struct _pk_string name; // service name
};

struct pk_response {
	struct pk_keepalive _super;
	uint16_t services;
};

// a service info response packet - PK_SERVICE
struct pk_service {
	struct pk_keepalive _super;
	struct _pk_address address; // host address
	uint16_t port; // port number
	uint32_t reserved; // reserved - for future use
	struct _pk_string name; // service name
};

#pragma pack(pop)

typedef enum pk_type pk_type_t;
typedef struct pk_keepalive pk_keepalive_t;
typedef struct pk_advertize pk_advertize_t;
typedef struct pk_response pk_response_t;
typedef struct pk_service pk_service_t;

ssize_t pk_send(int sockfd, pk_keepalive_t * pk, int flags);
void hton_pk(pk_keepalive_t * pk);
ssize_t pk_recv(int sockfd, char buf[UINT8_MAX + 1], int flags);
void ntoh_pk(pk_keepalive_t * pk);

int open_socket(char * hostname, char * servname, int * sockfd);

pk_keepalive_t * alloc_packet(unsigned long size);
void init_packet(pk_keepalive_t * pk, pk_type_t type);
void free_packet(pk_keepalive_t * pk);

pk_keepalive_t * make_pk_keepalive(pk_type_t type);
pk_advertize_t * make_pk_advertize(unsigned short port, const char * name);
pk_response_t * make_pk_response(unsigned short services);
pk_service_t * make_pk_service(struct in_addr address, unsigned short port, const char * name);
pk_service_t * make_pk_service6(struct in6_addr address, unsigned short port, const char * name);

#endif
