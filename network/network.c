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


void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
	
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void hton_addr(struct _pk_address * addr) {
	for (int i = 0; i < 4; i++)
		addr->data[i] = htonl(addr->data[i]);
}

void hton_str(struct _pk_string * str) {
	str->length = htonl(str->length);
}

void hton_pk(pk_keepalive_t * pk) {
	pk->netver = htonl(pk->netver);
	pk->size = htonl(pk->size);
	
	switch (pk->type) {
		case PK_ADVERTIZE:
			((pk_advertize_t *)pk)->port = htons(((pk_advertize_t *)pk)->port);
			((pk_advertize_t *)pk)->reserved = htonl(((pk_advertize_t *)pk)->reserved);
			hton_str(&(((pk_advertize_t *)pk)->name));
			break;
		case PK_SERVICE:
			hton_addr(&(((pk_service_t *)pk)->address));
			((pk_service_t *)pk)->port = htons(((pk_service_t *)pk)->port);
			((pk_service_t *)pk)->reserved = htonl(((pk_service_t *)pk)->reserved);
			hton_str(&(((pk_service_t *)pk)->name));
			break;
		default:
			break;
	}
}

void ntoh_addr(struct _pk_address * addr) {
	for (int i = 0; i < 4; i++)
		addr->data[i] = ntohl(addr->data[i]);
}

void ntoh_str(struct _pk_string * str) {
	str->length = ntohs(str->length);
}

void ntoh_pk(pk_keepalive_t * pk) {
	pk->netver = ntohs(pk->netver);
	pk->size = ntohs(pk->size);
	
	switch (pk->type) {
		case PK_ADVERTIZE:
			((pk_advertize_t *)pk)->port = ntohs(((pk_advertize_t *)pk)->port);
			((pk_advertize_t *)pk)->reserved = ntohl(((pk_advertize_t *)pk)->reserved);
			ntoh_str(&(((pk_advertize_t *)pk)->name));
			break;
		case PK_SERVICE:
			ntoh_addr(&(((pk_service_t *)pk)->address));
			((pk_service_t *)pk)->port = ntohs(((pk_service_t *)pk)->port);
			((pk_service_t *)pk)->reserved = ntohl(((pk_service_t *)pk)->reserved);
			ntoh_str(&(((pk_service_t *)pk)->name));
			break;
		default:
			break;
	}
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

pk_keepalive_t * alloc_packet(unsigned long size) {
	if (size < UINT32_MAX)
		return 0;
	
	pk_keepalive_t * pk = calloc(1, size);
	
	pk->size = (uint32_t)size;
	
	return pk;
}

void init_packet(pk_keepalive_t * pk, unsigned char type) {
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

void init_string(struct _pk_string * dest, const char * src) {
	dest->length = (uint32_t)strlen(src) + 1;
	strcpy((char *)&dest->data, src);
}

void init_address(struct _pk_address * dest, struct in_addr * src) {
	dest->family = AF_INET;
	dest->data[0] = src->s_addr;
	dest->data[1] = 0;
	dest->data[2] = 0;
	dest->data[3] = 0;
}

void init_address6(struct _pk_address * dest, struct in6_addr * src) {
	dest->family = AF_INET6;
	for (int i = 0; i < 4; i++)
		dest->data[i] = src->__u6_addr.__u6_addr32[i];
}

pk_keepalive_t * make_pk_keepalive(unsigned char type) {
	pk_keepalive_t * pk = alloc_packet(sizeof(pk_keepalive_t));
	init_packet(pk, type);
	return pk;
}

pk_advertize_t * make_pk_advertize(unsigned short port, const char * name) {
	unsigned long length = strlen(name);
	unsigned long size = sizeof(pk_advertize_t);
	
	pk_advertize_t * ad = (pk_advertize_t *)alloc_packet(size + length);
	if (!ad)
		return ad;
	
	init_packet((pk_keepalive_t *)ad, PK_ADVERTIZE);
	ad->port = port;
	init_string(&ad->name, name);
	
	return ad;
}

pk_service_t * make_pk_service(struct in_addr address, unsigned short port, const char * name) {
	pk_service_t * serv = (pk_service_t *)alloc_packet(sizeof(pk_advertize_t) + strlen(name));
	if (!serv)
		return serv;
	
	init_packet((pk_keepalive_t *)serv, PK_ADVERTIZE);
	init_address(&serv->address, &address);
	serv->port = port;
	init_string(&serv->name, name);
	
	return serv;
}

pk_service_t * make_pk_service6(struct in6_addr address, unsigned short port, const char * name) {
	pk_service_t * serv = (pk_service_t *)alloc_packet(sizeof(pk_advertize_t) + strlen(name));
	if (!serv)
		return serv;
	
	init_packet((pk_keepalive_t *)serv, PK_ADVERTIZE);
	init_address6(&serv->address, &address);
	serv->port = port;
	init_string(&serv->name, name);
	
	return serv;
}











