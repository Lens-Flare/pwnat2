//
//  network.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
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
	str->len = htonl(str->len);
}

void hton_pk(pk_keepalive_t * pk) {
	pk->type = htonl(pk->type);
	pk->size = htonl(pk->size);
	
	switch (pk->type) {
		case PK_KEEPALIVE:
			break;
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
		case PK_REQUEST:
			break;
		case PK_FORWARD:
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
	str->len = htonl(str->len);
}

void ntoh_pk(pk_keepalive_t * pk) {
	pk->type = htonl(pk->type);
	pk->size = htonl(pk->size);
	
	switch (pk->type) {
		case PK_KEEPALIVE:
			break;
		case PK_ADVERTIZE:
			((pk_advertize_t *)pk)->port = ntohs(((pk_advertize_t *)pk)->port);
			((pk_advertize_t *)pk)->reserved = ntohl(((pk_advertize_t *)pk)->reserved);
			hton_str(&(((pk_advertize_t *)pk)->name));
			break;
		case PK_SERVICE:
			hton_addr(&(((pk_service_t *)pk)->address));
			((pk_service_t *)pk)->port = ntohs(((pk_service_t *)pk)->port);
			((pk_service_t *)pk)->reserved = ntohl(((pk_service_t *)pk)->reserved);
			hton_str(&(((pk_service_t *)pk)->name));
			break;
		case PK_REQUEST:
			break;
		case PK_FORWARD:
			break;
		default:
			break;
	}
}