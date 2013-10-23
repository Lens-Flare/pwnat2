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
#include <sqlite3.h>
#include "../common/common.h"

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

#define NET_VER			2
#define PACKET_SIG		0xB6
#define PACKET_SIZE_MAX	UINT8_MAX
#define STRING_SIZE_MAX	(PACKET_SIZE_MAX - sizeof(pk_service_t))

enum pk_type {
	PK_KEEPALIVE,
	PK_BADSWVER,
	PK_BADNETVER,
	PK_BADPACKET,
	PK_HANDSHAKE,
	PK_ADVERTIZE,
	PK_REQUEST,
	PK_SERVICE,
	PK_RESPONSE,
	PK_FORWARD,
	PK_EXITING
};

enum pk_hs_step {
	PK_HS_INITIAL,
	PK_HS_ACKNOWLEDGE,
	PK_HS_FINAL
};

enum pk_error_code {
	SUCCESS = 0,
	
	NET_ERR_SERVER_BAD_SOFTWARE_VERSION = 1,
	NET_ERR_SERVER_BAD_NETWORK_VERSION,
	NET_ERR_BAD_MAJOR_VERSION,
	NET_ERR_BAD_MINOR_VERSION,
	NET_ERR_BAD_REVISION,
	NET_ERR_BAD_SUBREVISION,
	NET_ERR_BAD_NETWORK_VERSION,
	
	NET_ERR_HANDSHAKE_DATA_IS_NOT_HASH = 1,
	NET_ERR_HANDSHAKE_INCORRECT_HASH,
	NET_ERR_HANDSHAKE_BAD_SEQUENCE
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
	uint8_t signature; // pwnat packet signature
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
typedef enum pk_error_code pk_error_code_t;
typedef struct pk_keepalive pk_keepalive_t;
typedef struct pk_handshake pk_handshake_t;
typedef struct pk_advertize pk_advertize_t;
typedef struct pk_service pk_service_t;

ssize_t pk_send(int sockfd, pk_keepalive_t * pk, int flags);
void hton_pk(pk_keepalive_t * pk);
ssize_t pk_recv(int sockfd, char buf[PACKET_SIZE_MAX], unsigned int timeout, int flags);
void ntoh_pk(pk_keepalive_t * pk);

int sqlite3_bind_address(sqlite3_stmt * stmt, int index, struct sockaddr * sa);
void * get_in_addr(struct sockaddr *sa);
const char * get_port_service_name(int port, const char * proto);
int listen_socket(const char * hostname, const char * servname, int backlog, int * sockfd);
int connect_socket(const char * hostname, const char * servname, int * sockfd);

pk_keepalive_t * alloc_packet(unsigned long size);
void init_packet(pk_keepalive_t * pk, pk_type_t type);
void free_packet(pk_keepalive_t * pk);

pk_keepalive_t * make_pk_keepalive(pk_type_t type);
pk_handshake_t * make_pk_handshake(pk_handshake_t * recv);
pk_advertize_t * make_pk_advertize(unsigned short port, const char * name);
pk_service_t * make_pk_service(struct sockaddr * address, unsigned short port, const char * name);

void init_pk_advertize(pk_advertize_t * ad, unsigned short port, const char * name);
void init_pk_service(pk_service_t * serv, struct sockaddr * address, unsigned short port, const char * name);

pk_error_code_t check_version(pk_keepalive_t * pk);
pk_error_code_t check_handshake(pk_handshake_t * hs, pk_handshake_t * recv);

errcode send_handshake(int sockfd, int timeout);
errcode recv_handshake(int sockfd, int timeout);

#endif
