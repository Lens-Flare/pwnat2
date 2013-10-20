//
//  network.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "network.h"

#ifdef __APPLE__
	#define pk_hs_hash(dest, src) CC_SHA256(src, HANDSHAKE_SIZE, dest);
#else
	#define pk_hs_hash(dest, src) SHA256(src, HANDSHAKE_SIZE, dest);
#endif

#pragma mark Packet Transmission/Reception

ssize_t pk_send(int sockfd, pk_keepalive_t * pk, int flags) {
	ssize_t retv;
	
	if (!pk)
		return 0;
	
	hton_pk(pk);
	retv = send(sockfd, pk, pk->size, flags);
	ntoh_pk(pk);
	if (retv >= 0 && retv != pk->size) {
		fprintf(stderr, "%zd bytes of %d bytes sent\n", retv, pk->size);
		retv = -1;
	}
	
	return retv;
}

static void hton_addr(struct _pk_address * addr) {
	for (int i = 0; i < 4; i++)
		addr->data[i] = htonl(addr->data[i]);
}

void hton_pk(pk_keepalive_t * pk) {
	pk->netver = htonl(pk->netver);
	
	switch (pk->type) {
		case PK_ADVERTIZE:
			((pk_advertize_t *)pk)->port = htons(((pk_advertize_t *)pk)->port);
			((pk_advertize_t *)pk)->reserved = htonl(((pk_advertize_t *)pk)->reserved);
			break;
		case PK_RESPONSE:
			((pk_response_t *)pk)->services = htons(((pk_response_t *)pk)->services);
			break;
		case PK_SERVICE:
			hton_addr(&(((pk_service_t *)pk)->address));
			((pk_service_t *)pk)->port = htons(((pk_service_t *)pk)->port);
			((pk_service_t *)pk)->reserved = htonl(((pk_service_t *)pk)->reserved);
			break;
		default:
			break;
	}
}

ssize_t pk_recv(int sockfd, char buf[PACKET_SIZE_MAX], int flags) {
	ssize_t retv;
	
	if ((retv = recv(sockfd, buf, PACKET_SIZE_MAX, flags)) >= 0) {
		pk_keepalive_t * pk = (pk_keepalive_t *)buf;
		ntoh_pk(pk);
		if (retv >= 0 && retv != pk->size) {
			fprintf(stderr, "%ld bytes received but packet size is %d bytes\n", retv, pk->size);
			retv = -1;
		}
	}
	
	return retv;
}

static void ntoh_addr(struct _pk_address * addr) {
	for (int i = 0; i < 4; i++)
		addr->data[i] = ntohl(addr->data[i]);
}

void ntoh_pk(pk_keepalive_t * pk) {
	pk->netver = ntohs(pk->netver);
	
	switch (pk->type) {
		case PK_ADVERTIZE:
			((pk_advertize_t *)pk)->port = ntohs(((pk_advertize_t *)pk)->port);
			((pk_advertize_t *)pk)->reserved = ntohl(((pk_advertize_t *)pk)->reserved);
			break;
		case PK_RESPONSE:
			((pk_response_t *)pk)->services = ntohs(((pk_response_t *)pk)->services);
			break;
		case PK_SERVICE:
			ntoh_addr(&(((pk_service_t *)pk)->address));
			((pk_service_t *)pk)->port = ntohs(((pk_service_t *)pk)->port);
			((pk_service_t *)pk)->reserved = ntohl(((pk_service_t *)pk)->reserved);
			break;
		default:
			break;
	}
}

#pragma mark - Generic Network Functions

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int open_socket(const char * hostname, const char * servname, int * sockfd) {
	int retv, yes;
	struct addrinfo hints, *servinfo, *p;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if (!(retv = getaddrinfo(hostname, servname, &hints, &servinfo))) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retv));
		return 1;
	}
	
	for (p = servinfo; p; p = p->ai_next) {
		if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			perror("server: socket");
			continue;
		}
		
		if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
			perror("setsockopt");
			return 1;
		}
		
		if (bind(*sockfd, p->ai_addr, p->ai_addrlen) < 0) {
			close(*sockfd);
			perror("server: bind");
			continue;
		}
		
		break;
	}
	
	if (!p)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}
	
	freeaddrinfo(servinfo);
	
	return 0;
}

#pragma mark - Packet Struct Lifecycle

pk_keepalive_t * alloc_packet(unsigned long size) {
	if (size < PACKET_SIZE_MAX)
		return 0;
	
	pk_keepalive_t * pk = calloc(1, size);
	if (!pk) {
		perror("calloc");
		return pk;
	}
	
	pk->size = (uint8_t)size;
	
	return pk;
}

void init_packet(pk_keepalive_t * pk, pk_type_t type) {
	pk->version[0] = MAJOR_VERSION;
	pk->version[1] = MINOR_VERSION;
	pk->version[2] = REVISION;
	pk->version[3] = SUBREVISION;
	
	pk->netver = NET_VER;
	
	pk->type = type;
}

void free_packet(pk_keepalive_t * pk) {
	free(pk);
}

#pragma mark - Packet Struct Helper Functions

static void pkcpy_string(struct _pk_string * dest, const char * src) {
	dest->length = (uint8_t)strlen(src) + 1;
	strcpy((char *)&dest->data, src);
}

static void pkcpy_address(struct _pk_address * dest, struct in_addr * src) {
	dest->family = AF_INET;
	dest->data[0] = src->s_addr;
	dest->data[1] = 0;
	dest->data[2] = 0;
	dest->data[3] = 0;
}

static void pkcpy_address6(struct _pk_address * dest, struct in6_addr * src) {
	dest->family = AF_INET6;
	for (int i = 0; i < 4; i++)
		#ifdef __APPLE__
			dest->data[i] = src->__u6_addr.__u6_addr32[i];
		#elif __linux
			dest->data[i] = src->__in6_u.__u6_addr32[i];
		#else
			#error "Architecture unsupported - in6_addr data unknown"
		#endif
}

#pragma mark - Packet Struct Construction

pk_keepalive_t * make_pk_keepalive(pk_type_t type) {
	pk_keepalive_t * pk = alloc_packet(sizeof(pk_keepalive_t));
	if (!pk)
		return pk;
	
	init_packet(pk, type);
	
	return pk;
}

pk_handshake_t * make_pk_handshake(pk_handshake_t * recv) {
	pk_handshake_t * hs = (pk_handshake_t *)alloc_packet(sizeof(pk_handshake_t));
	if (!hs)
		return hs;
	
	init_packet((pk_keepalive_t *)hs, PK_HANDSHAKE);
	
	if (!recv) {
		hs->step = PK_HS_INITIAL;
		_random(&hs->data, HANDSHAKE_SIZE);
		pk_hs_hash((void *)&hs->hash, &hs->data);
	} else {
		switch (recv->step) {
			case PK_HS_INITIAL:
				hs->step = PK_HS_ACKNOWLEDGE;
				break;
			case PK_HS_ACKNOWLEDGE:
				hs->step = PK_HS_FINAL;
				break;
			default:
				free(hs);
				return NULL;
				break;
		}
		memcpy(&hs->data, &recv->hash, HANDSHAKE_SIZE);
		pk_hs_hash((void *)&hs->hash, &hs->data);
	}
	
	return hs;
}

pk_advertize_t * make_pk_advertize(unsigned short port, const char * name) {
	pk_advertize_t * ad = (pk_advertize_t *)alloc_packet(sizeof(pk_advertize_t) + strlen(name));
	if (!ad)
		return ad;
	
	init_packet((pk_keepalive_t *)ad, PK_ADVERTIZE);
	ad->port = port;
	pkcpy_string(&ad->name, name);
	
	return ad;
}

pk_response_t * make_pk_response(unsigned short services) {
	pk_response_t * rsp = (pk_response_t *)alloc_packet(sizeof(pk_response_t));
	if (!rsp)
		return rsp;
	
	init_packet((pk_keepalive_t *)rsp, PK_RESPONSE);
	rsp->services = services;
	
	return rsp;
}

pk_service_t * make_pk_service(struct in_addr address, unsigned short port, const char * name) {
	pk_service_t * serv = (pk_service_t *)alloc_packet(sizeof(pk_advertize_t) + strlen(name));
	if (!serv)
		return serv;
	
	init_packet((pk_keepalive_t *)serv, PK_ADVERTIZE);
	pkcpy_address(&serv->address, &address);
	serv->port = port;
	pkcpy_string(&serv->name, name);
	
	return serv;
}

pk_service_t * make_pk_service6(struct in6_addr address, unsigned short port, const char * name) {
	pk_service_t * serv = (pk_service_t *)alloc_packet(sizeof(pk_advertize_t) + strlen(name));
	if (!serv)
		return serv;
	
	init_packet((pk_keepalive_t *)serv, PK_ADVERTIZE);
	pkcpy_address6(&serv->address, &address);
	serv->port = port;
	pkcpy_string(&serv->name, name);
	
	return serv;
}

#pragma mark - Packet Checking

errcode check_version(pk_keepalive_t * pk) {
	switch (pk->type) {
		case PK_BADSWVER:
			fprintf(stderr, "Server says incompatible software version (%d.%d-r%d.%d)", pk->version[0], pk->version[1], pk->version[2], pk->version[3]);
			return 1;
			break;
		case PK_BADNETVER:
			fprintf(stderr, "Server says incompatible network structure version (%d)", pk->netver);
			return 2;
			break;
		default:
			if (pk->version[0] != MAJOR_VERSION)
				return 3;
			if (pk->version[1] != MINOR_VERSION)
				return 4;
			if (pk->version[2] != REVISION)
				return 5;
			if (pk->version[3] != SUBREVISION)
				return 6;
			if (pk->netver != NET_VER)
				return 7;
			return 0;
			break;
	}
}

errcode check_handshake(pk_handshake_t * hs, pk_handshake_t * recv) {
	uint8_t check[HANDSHAKE_SIZE];
	
	if (hs) {
		if (!memcmp(&hs->hash, &recv->data, HANDSHAKE_SIZE))
			return 1;
		if ((hs->step == PK_HS_INITIAL && recv->step != PK_HS_ACKNOWLEDGE) ||
			(hs->step == PK_HS_ACKNOWLEDGE && recv->step != PK_HS_FINAL))
			return 3;
	} else if (recv->step != PK_HS_INITIAL)
		return 3;
	
	pk_hs_hash(check, &recv->data);
	if (!memcmp(&recv->hash, check, HANDSHAKE_SIZE))
		return 2;
	
	return 0;
}

#pragma mark - Handshaking

#define IF_GOTO_EXIT(check) if ((retv = check)) goto exit;

errcode send_handshake(int sockfd) {
	errcode retv = 0;
	char buf[256];
	pk_handshake_t * hs, * recv = (pk_handshake_t *)buf;
	
	IF_GOTO_EXIT(!(hs = make_pk_handshake(NULL)));
	IF_GOTO_EXIT(pk_send(sockfd, (pk_keepalive_t *)hs, 0) < 0);
	
	IF_GOTO_EXIT(pk_recv(sockfd, buf, 0) < 0);
	IF_GOTO_EXIT(check_version((pk_keepalive_t *)recv));
	IF_GOTO_EXIT(check_handshake(hs, recv));
	
	free(hs);
	IF_GOTO_EXIT(!(hs = make_pk_handshake(recv)));
	IF_GOTO_EXIT(pk_send(sockfd, (pk_keepalive_t *)hs, 0) < 0);
	
exit:
	free_packet((pk_keepalive_t *)hs);
	return retv;
}

errcode recv_handshake(int sockfd) {
	errcode retv = 0;
	char buf[256];
	pk_handshake_t * hs = NULL, * recv = (pk_handshake_t *)buf;
	
	IF_GOTO_EXIT(pk_recv(sockfd, buf, 0) < 0);
	IF_GOTO_EXIT(check_version((pk_keepalive_t *)recv));
	IF_GOTO_EXIT(check_handshake(hs, recv));
	
	IF_GOTO_EXIT(!(hs = make_pk_handshake(recv)));
	IF_GOTO_EXIT(pk_send(sockfd, (pk_keepalive_t *)hs, 0) < 0);
	
	IF_GOTO_EXIT(pk_recv(sockfd, buf, 0) < 0);
	IF_GOTO_EXIT(check_version((pk_keepalive_t *)recv));
	IF_GOTO_EXIT(check_handshake(hs, recv));
	
exit:
	free_packet((pk_keepalive_t *)hs);
	return retv;
}