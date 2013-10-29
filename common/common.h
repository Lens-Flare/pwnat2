//
//  common.h
//  pwnat2
//
//  Created by Ethan Reesor on 10/19/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#ifndef pwnat2_common_h
#define pwnat2_common_h

#include <stdint.h>
#include <netdb.h>
#include <sqlite3.h>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#define SHA256(dest, src, n) CC_SHA256((const void *) src, n, (void *) dest);
#else
#include <openssl/sha.h>
#define SHA256(dest, src, n) SHA256((const void *) src, n, (void *) dest);
#endif

#pragma mark - Macros

#define ARRLEN(s)					(sizeof(s)/sizeof(s[0]))
#define _perror()					perror(__FUNCTION__)
#define HANDSHAKE_HASH(dest, src)	SHA256(dest, src, HANDSHAKE_SIZE);



#pragma mark - Constants

#define MAJOR_VERSION	0
#define MINOR_VERSION	1
#define REVISION		1
#define SUBREVISION		0

#define SERVER_PORT		"45678"
#define SERVER_DB_TMP	"/tmp/pwnat.db.XXX"
#define DEFAULT_TIMEOUT	60
#define ENV_PREFIX		"_PWNAT_"
#define ENV_PREFIX_LEN	ARRLEN(ENV_PREFIX)



#pragma mark - Constants/Network

#ifdef __APPLE__
#define HANDSHAKE_SIZE CC_SHA256_DIGEST_LENGTH
#else
#define HANDSHAKE_SIZE SHA256_DIGEST_LENGTH
#endif

#define NET_VER			3
#define PACKET_SIG		0xB6
#define PACKET_SIZE_MAX	UINT8_MAX
#define STRING_SIZE_MAX	(PACKET_SIZE_MAX - sizeof(pk_service_t))



#pragma mark - Types

typedef enum {TRUE = 1, FALSE = 0} boolean;
typedef int errcode;



#pragma mark - Types/Config

typedef intptr_t cfgint;

struct config_var {
	char * name;			// long option
	struct {
		uint8_t flag:1;		// true if this option is a flag - implies numeric and no argument
		uint8_t required:1;	// true if this option requires an argument
		uint8_t numeric:1;	// true if this option is numeric
	} type;
	char short_opt;			// short option
	char * env_name;		// environment variable name
	void * default_val;		// default value - if numeric, cfgint, otherwise, char *
	void * value;			// variable value - if numeric, cfgint *, otherwise, char **
	
	// CAVEAT: int values must be castable to char *; if the size of a pointer
	//   is larger than the size of the value storage, nearby values will be
	//   overwritten
};

enum config_type {
	ct_var = 0,
	ct_flg,
	ct_opt = 0,
	ct_req,
	ct_str = 0,
	ct_num,
};



#pragma mark - Types/Network

enum pk_type {
	PK_KEEPALIVE,	// keepalive
	PK_BADSWVER,	// bad software version
	PK_BADNETVER,	// bad network version
	PK_BADPACKET,	// bad packet type
	PK_BADADDR,		// bad address
	PK_FWNOTRDY,	// forward target not ready
	PK_HANDSHAKE,	// connection handshake
	PK_ADVERTIZE,	// service advertizement
	PK_REQUEST,		// service list request
	PK_SERVICE,		// service list element
	PK_RESPONSE,	// service list terminator
	PK_FORWARD,		// forward request
	PK_EXITING,		// exiting notification
	PK_MISCDATA		// miscelaneous data
};

enum pk_hs_step {
	PK_HS_INITIAL,
	PK_HS_ACKNOWLEDGE,
	PK_HS_FINAL
};

enum pk_error_code {
	SUCCESS = 0,
	
	PK_RECV_CONN_CLOSED = 1,
	PK_RECV_MISSING_SIG,
	PK_RECV_ERROR,
	PK_RECV_BAD_SIZE,
	
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

// IP address
struct _pk_address {				// IP_ADDR
	uint8_t family;					// equals AF_INET or AF_INET6
	uint32_t data[4];				// cast to in_addr or in6_addr
};

// string
struct _pk_string {					// STRING
	uint8_t length;					// number of bytes
	uint8_t data[];					// bytes (null terminated)
};

// keepalive packet
struct pk_keepalive {				// PK_KEEPALIVE,PK_BADSWVER,PK_BADNETVER,PK_BADPACKET,PK_REQUEST,PK_RESPONSE,PK_EXITING
	uint8_t signature;				// pwnat packet signature
	uint8_t version[4];				// software version
	uint32_t netver;				// network structure version
	uint8_t type;					// packet type
	uint8_t size;					// packet size
};

// handshake packet
struct pk_handshake {
	struct pk_keepalive _super;		// PK_HANDSHAKE
	uint8_t step;					// handshake step
	uint8_t data[HANDSHAKE_SIZE];	// data
	uint8_t hash[HANDSHAKE_SIZE];	// hash of data
};

// service advertizing packet
struct pk_advertize {
	struct pk_keepalive _super;		// PK_ADVERTIZE
	uint16_t port;					// port number
	uint32_t reserved;				// reserved - for future use
	struct _pk_string name;			// service name
};

// service info response packet
struct pk_service {
	struct pk_keepalive _super;		// PK_SERVICE
	struct _pk_address address;		// host address
	uint16_t port;					// port number
	uint32_t reserved;				// reserved - for future use
	struct _pk_string name;			// service name
};

// packet forward packet
struct pk_forward {
	struct pk_keepalive _super;		// PK_FORWARD,PK_BADADDR,PK_FWNOTRDY
	struct _pk_address address;		// destination address
	uint16_t port;					// port number
	struct _pk_string packet;		// packet to forward
};

// miscelaneous data packet
struct pk_miscdata {
	struct pk_keepalive _super;		// PK_MISCDATA
	struct _pk_string miscdata;		// data
};

#pragma pack(pop)

typedef enum pk_type pk_type_t;
typedef enum pk_hs_step pk_hs_step_t;
typedef enum pk_error_code pk_error_code_t;
typedef struct pk_keepalive pk_keepalive_t;
typedef struct pk_handshake pk_handshake_t;
typedef struct pk_advertize pk_advertize_t;
typedef struct pk_service pk_service_t;
typedef struct pk_forward pk_forward_t;
typedef struct pk_miscdata pk_miscdata_t;



#pragma mark - Functions

void _sleep(int millis);
int _fork(pid_t * cpid, int (*run)(void *), void * params);
int _random(void * data, size_t len);
void _waitpid_sigchld_handler(int s);



#pragma mark - Functions/Config

int config(int argc, const char * argv[], int num_vars, struct config_var * vars);



#pragma mark - Functions/Network/TxRx

ssize_t pk_send(int sockfd, pk_keepalive_t * pk, int flags);
void hton_pk(pk_keepalive_t * pk);
ssize_t pk_recv(int sockfd, char buf[PACKET_SIZE_MAX], unsigned int timeout, int flags);
void ntoh_pk(pk_keepalive_t * pk);



#pragma mark - Functions/Network/Connect

void * get_in_addr(struct sockaddr * sa);
in_port_t get_in_port(struct sockaddr * sa);
size_t get_in_addr_len(struct sockaddr * sa);

uint64_t get_addrptr_port_hash(void * addr, sa_family_t family, in_port_t port);
uint64_t get_sock_addr_hash(struct sockaddr * sa);
uint64_t get_addr_port_hash(struct _pk_address * addr, in_port_t port);

const char * get_port_service_name(int port, const char * proto);

int listen_socket(const char * hostname, const char * servname, int backlog, int * sockfd);
int connect_socket(const char * hostname, const char * servname, int * sockfd);



#pragma mark - Functions/Network/Packet

pk_keepalive_t * alloc_packet(unsigned long size);
void init_packet(pk_keepalive_t * pk, pk_type_t type);
void free_packet(pk_keepalive_t * pk);

pk_keepalive_t * make_pk_keepalive(pk_type_t type);
pk_handshake_t * make_pk_handshake(pk_handshake_t * recv);
pk_advertize_t * make_pk_advertize(in_port_t port, const char * name);
pk_service_t * make_pk_service(struct sockaddr * address, in_port_t port, const char * name);
pk_forward_t * make_pk_forward(struct sockaddr * address, in_port_t port);
pk_miscdata_t * make_pk_miscdata(const char * buf, size_t len);

void init_pk_advertize(pk_advertize_t * ad, in_port_t port, const char * name);
void init_pk_service(pk_service_t * serv, struct sockaddr * address, in_port_t port, const char * name);
void init_pk_forward(pk_forward_t * fw, struct sockaddr * address, in_port_t port);
void init_pk_miscdata(pk_miscdata_t * pk, const char * buf, size_t len);



#pragma mark - Functions/Network/Verification

pk_error_code_t check_version(pk_keepalive_t * pk);
pk_error_code_t check_handshake(pk_handshake_t * hs, pk_handshake_t * recv);

errcode send_handshake(int sockfd, int timeout);
errcode recv_handshake(int sockfd, int timeout);

#endif
