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

#ifdef __APPLE__
	#include <CommonCrypto/CommonDigest.h>
#else
	#include <openssl/sha.h>
#endif

#ifdef __APPLE__
	#define HANDSHAKE_SIZE CC_SHA256_DIGEST_LENGTH
#else
	#define HANDSHAKE_SIZE SHA256_DIGEST_LENGTH
#endif

#define NET_VER			1
#define PACKET_SIZE_MAX	UINT8_MAX

enum pk_type {
	PK_KEEPALIVE,
	PK_BADSWVER,
	PK_BADNETVER,
	PK_HANDSHAKE,
	PK_ADVERTIZE,
	PK_REQUEST,
	PK_RESPONSE,
	PK_SERVICE,
	PK_FORWARD
};

enum pk_hs_step {
	PK_HS_INITIAL,
	PK_HS_ACKNOWLEDGE,
	PK_HS_FINAL
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

// a handshake packet
struct pk_handshake {
	struct pk_keepalive _super;
	uint8_t step;
	uint8_t data[HANDSHAKE_SIZE]; // data
	uint8_t hash[HANDSHAKE_SIZE]; // hash of data
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
typedef enum pk_hs_step pk_hs_step_t;
typedef struct pk_keepalive pk_keepalive_t;
typedef struct pk_handshake pk_handshake_t;
typedef struct pk_advertize pk_advertize_t;
typedef struct pk_response pk_response_t;
typedef struct pk_service pk_service_t;

ssize_t pk_send(int sockfd, pk_keepalive_t * pk, int flags);
void hton_pk(pk_keepalive_t * pk);
ssize_t pk_recv(int sockfd, char buf[PACKET_SIZE_MAX], int flags);
void ntoh_pk(pk_keepalive_t * pk);

int open_socket(char * hostname, char * servname, int * sockfd);

pk_keepalive_t * alloc_packet(unsigned long size);
void init_packet(pk_keepalive_t * pk, pk_type_t type);
void free_packet(pk_keepalive_t * pk);

pk_keepalive_t * make_pk_keepalive(pk_type_t type);
pk_handshake_t * make_pk_handshake(pk_handshake_t * recv);
pk_advertize_t * make_pk_advertize(unsigned short port, const char * name);
pk_response_t * make_pk_response(unsigned short services);
pk_service_t * make_pk_service(struct in_addr address, unsigned short port, const char * name);
pk_service_t * make_pk_service6(struct in6_addr address, unsigned short port, const char * name);

int check_handshake(pk_handshake_t * recv);

#endif
