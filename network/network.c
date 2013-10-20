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
#include "common.h"

#pragma mark Packet Network/Host-form Conversion

void hton_addr(struct _pk_address * addr) {
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

void ntoh_addr(struct _pk_address * addr) {
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

int open_socket(char * hostname, char * servname, int * sockfd) {
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
	if (size < UINT8_MAX)
		return 0;
	
	pk_keepalive_t * pk = calloc(1, size);
	if (!pk) {
		_perror();
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











