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

int open_socket(char * hostname, char * servname, int backlog, int * sockfd) {
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