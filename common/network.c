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
#include <arpa/inet.h>
#include <errno.h>
#include <sqlite3.h>

#include "common.h"



#pragma mark Packet Transmission/Reception

ssize_t pk_send(int sockfd, pk_keepalive_t * pk, int flags) {
	ssize_t retv;
	
	if (!pk)
		return 0;
	
	hton_pk(pk);
	retv = send(sockfd, pk, pk->size, flags);
	ntoh_pk(pk);
	
	if (retv < 0) {
		perror("send");
	} else if (retv != pk->size) {
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
		case PK_SERVICE:
			hton_addr(&(((pk_service_t *)pk)->address));
			((pk_service_t *)pk)->port = htons(((pk_service_t *)pk)->port);
			((pk_service_t *)pk)->reserved = htonl(((pk_service_t *)pk)->reserved);
			break;
		default:
			break;
	}
}


ssize_t pk_recv(int sockfd, char buf[PACKET_SIZE_MAX], unsigned int timeout, int flags) {
	ssize_t bytes = 0, total = 0;
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	
	buf[0] = 0;
	alarm(timeout);
	
	total += bytes = recv(sockfd, buf, 1, flags);
	
	if (bytes < 0) {
		perror("recv");
		total = -PK_RECV_ERROR;
		goto _return;
	} else if (bytes == 0) {
		fprintf(stderr, "Connection closed\n");
		total = -PK_RECV_CONN_CLOSED;
		goto _return;
	} else if (buf[0] != (char)PACKET_SIG) {
		fprintf(stderr, "Missing packet signature\n");
		total = -PK_RECV_MISSING_SIG;
		goto _return;
	}
	
	total += bytes = recv(sockfd, buf + total, sizeof(pk_keepalive_t) - total, flags);
	
	if (bytes < 0) {
		perror("recv");
		total = -PK_RECV_ERROR;
		goto _return;
	} else if (pk->size == total)
		goto done;
	else if (pk->size < total) {
		fprintf(stderr, "%ld bytes received but packet size is %d bytes\n", total, pk->size);
		total = -PK_RECV_BAD_SIZE;
		goto _return;
	}
	
	total += bytes = recv(sockfd, buf + total, pk->size - total, flags);
	
	if (bytes < 0) {
		perror("recv");
		total = -PK_RECV_ERROR;
		goto _return;
	} else if (pk->size < total) {
		fprintf(stderr, "%ld bytes received but packet size is %d bytes\n", total, pk->size);
		total = -PK_RECV_BAD_SIZE;
		goto _return;
	}
	
done:
	ntoh_pk(pk);
	if (pk->type == PK_ADVERTIZE || pk->type == PK_SERVICE) {
		struct _pk_string * str = (pk->type == PK_ADVERTIZE) ? &((pk_advertize_t *)pk)->name : &((pk_service_t *)pk)->name;
		
		if (str->length > 1 || (str->data[0] != '-' && str->data[0] != '+'))
			str->data[str->length - 1] = 0;
	}
	
_return:
	alarm(0);
	return total;
}

static void ntoh_addr(struct _pk_address * addr) {
	for (int i = 0; i < 4; i++)
		addr->data[i] = ntohl(addr->data[i]);
}

void ntoh_pk(pk_keepalive_t * pk) {
	pk->netver = ntohl(pk->netver);
	
	switch (pk->type) {
		case PK_ADVERTIZE:
			((pk_advertize_t *)pk)->port = ntohs(((pk_advertize_t *)pk)->port);
			((pk_advertize_t *)pk)->reserved = ntohl(((pk_advertize_t *)pk)->reserved);
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

void * get_in_addr(struct sockaddr * sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

in_port_t get_in_port(struct sockaddr * sa) {
	if (sa->sa_family == AF_INET)
		return ((struct sockaddr_in *)sa)->sin_port;
	return ((struct sockaddr_in6 *)sa)->sin6_port;
}

size_t get_in_addr_len(struct sockaddr * sa) {
	if (sa->sa_family == AF_INET)
		return sizeof(struct in_addr);
	return sizeof(struct in6_addr);
}

uint64_t get_addrptr_port_hash(void * addr, sa_family_t family, in_port_t port) {
	uint64_t hash[4];
	uint8_t data[sizeof(struct in6_addr) + 1];
	
	memset(data, 0, sizeof(data));
	
	if (family == AF_INET)
		memcpy(addr, data + 1, sizeof(struct in_addr));
	else
		memcpy(addr, data + 1, sizeof(struct in6_addr));
	
	SHA256(hash, data, sizeof(data));
	
	return hash[0];
}

uint64_t get_sock_addr_hash(struct sockaddr * sa) {
	if (sa->sa_family == AF_INET)
		return get_addrptr_port_hash(&sa->sa_data, sa->sa_family, ((struct sockaddr_in *)sa)->sin_port);
	else
		return get_addrptr_port_hash(&sa->sa_data, sa->sa_family, ((struct sockaddr_in6 *)sa)->sin6_port);
}

uint64_t get_addr_port_hash(struct _pk_address * addr, in_port_t port) {
	return get_addrptr_port_hash(&addr->data, addr->family, port);
}

const char * get_port_service_name(int port, const char * proto) {
	struct servent * serv = getservbyport(htons(port), proto);
	return serv ? serv->s_name : NULL;
}


int listen_socket(const char * hostname, const char * servname, int backlog, int * sockfd) {
	int retv, yes;
	struct addrinfo hints, *servinfo, *p;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((retv = getaddrinfo(hostname, servname, &hints, &servinfo))) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retv));
		return 1;
	}
	
	for (p = servinfo; p; p = p->ai_next) {
		char s[INET6_ADDRSTRLEN];
		inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
		printf("binding to %s\n", s);
		
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
	
	if (listen(*sockfd, backlog) == -1) {
		close(*sockfd);
		perror("listen");
		return 1;
	}
	
	return 0;
}

int connect_socket(const char * hostname, const char * servname, int * sockfd) {
	int retv;
	struct addrinfo hints, *servinfo, *p;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((retv = getaddrinfo(hostname, servname, &hints, &servinfo))) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retv));
		return 1;
	}
	
	for (p = servinfo; p; p = p->ai_next) {
		char s[INET6_ADDRSTRLEN];
		inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
		
		if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			perror("server: socket");
			continue;
		}
		
        if (connect(*sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(*sockfd);
            perror("client: connect");
            continue;
        }
		
		break;
	}
	
	if (!p)  {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}
	
	freeaddrinfo(servinfo);
	
	return 0;
}



#pragma mark - Packet Lifecycle

pk_keepalive_t * alloc_packet(unsigned long size) {
	if (size > PACKET_SIZE_MAX)
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
	pk->signature = 0xB6;
	
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



#pragma mark - Packet Helper Functions

static void pkcpy_string(struct _pk_string * dest, const char * src) {
	dest->length = (uint8_t)strlen(src) + 1;
	strcpy((char *)&dest->data, src);
}

static void pkcpy_address(struct _pk_address * dest, struct sockaddr * src) {
	dest->family = src->sa_family;
	
	if(src->sa_family == AF_INET)
		memcpy(dest->data, src->sa_data, sizeof(struct in_addr));
	else
		memcpy(dest->data, src->sa_data, sizeof(struct in6_addr));

//	for (int i = 0; i < 4 && i < src->sa_len; i++)
//		dest->data[i] = src->sa_data[i];
}



#pragma mark - Packet Construction

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
		HANDSHAKE_HASH((void *)&hs->hash, &hs->data);
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
		HANDSHAKE_HASH((void *)&hs->hash, &hs->data);
	}
	
	return hs;
}

pk_advertize_t * make_pk_advertize(in_port_t port, const char * name) {
	pk_advertize_t * ad = (pk_advertize_t *)alloc_packet(sizeof(pk_advertize_t) + strlen(name) + 1);
	if (!ad)
		return ad;
	
	init_pk_advertize(ad, port, name);
	
	return ad;
}

pk_service_t * make_pk_service(struct sockaddr * address, in_port_t port, const char * name) {
	pk_service_t * serv = (pk_service_t *)alloc_packet(sizeof(pk_advertize_t) + strlen(name) + 1);
	if (!serv)
		return serv;
	
	init_pk_service(serv, address, port, name);
	
	return serv;
}


pk_forward_t * make_pk_forward(struct sockaddr * address, in_port_t port) {
	pk_forward_t * fw = (pk_forward_t *)alloc_packet(sizeof(pk_forward_t));
	if (!fw)
		return fw;
	
	init_pk_forward(fw, address, port);
	
	return fw;
}

pk_miscdata_t * make_pk_miscdata(const char * buf, size_t len) {
	pk_miscdata_t * misc = (pk_miscdata_t *)alloc_packet(sizeof(pk_miscdata_t) + len);
	if (!misc)
		return misc;
	
	init_pk_miscdata(misc, buf, len);
	
	return misc;
}


void init_pk_advertize(pk_advertize_t * ad, in_port_t port, const char * name) {
	ad->_super.size = sizeof(pk_advertize_t) + strlen(name) + 1;
	
	init_packet((pk_keepalive_t *)ad, PK_ADVERTIZE);
	
	ad->port = port;
	if (name) pkcpy_string(&ad->name, name);
}

void init_pk_service(pk_service_t * serv, struct sockaddr * address, in_port_t port, const char * name) {
	serv->_super.size = sizeof(pk_service_t) + strlen(name) + 1;
	
	init_packet((pk_keepalive_t *)serv, PK_SERVICE);
	
	if (address) pkcpy_address(&serv->address, address);
	serv->port = port;
	if (name) pkcpy_string(&serv->name, name);
}

void init_pk_forward(pk_forward_t * fw, struct sockaddr * address, in_port_t port) {
//	serv->_super.size = sizeof(pk_forward_t);
	
	init_packet((pk_keepalive_t *)fw, PK_FORWARD);
	
	if (address) pkcpy_address(&fw->address, address);
	fw->port = port;
}

void init_pk_miscdata(pk_miscdata_t * misc, const char * buf, size_t len) {
	misc->_super.size = sizeof(pk_miscdata_t) + len;
	
	init_packet((pk_keepalive_t *)misc, PK_MISCDATA);
	
	if (buf) memcpy(&misc->miscdata.data, buf, len);
}



#pragma mark - Packet Checking

pk_error_code_t check_version(pk_keepalive_t * pk) {
	switch (pk->type) {
		case PK_BADSWVER:
			fprintf(stderr, "Server says incompatible software version (%d.%d-r%d.%d)", pk->version[0], pk->version[1], pk->version[2], pk->version[3]);
			return NET_ERR_SERVER_BAD_SOFTWARE_VERSION;
			break;
		case PK_BADNETVER:
			fprintf(stderr, "Server says incompatible network structure version (%d)", pk->netver);
			return NET_ERR_SERVER_BAD_NETWORK_VERSION;
			break;
		default:
			if (pk->version[0] != MAJOR_VERSION)
				return NET_ERR_BAD_MAJOR_VERSION;
			if (pk->version[1] != MINOR_VERSION)
				return NET_ERR_BAD_MINOR_VERSION;
			if (pk->version[2] != REVISION)
				return NET_ERR_BAD_REVISION;
			if (pk->version[3] != SUBREVISION)
				return NET_ERR_BAD_SUBREVISION;
			if (pk->netver != NET_VER)
				return NET_ERR_BAD_NETWORK_VERSION;
			break;
	}
	return SUCCESS;
}

pk_error_code_t check_handshake(pk_handshake_t * hs, pk_handshake_t * recv) {
	uint8_t check[HANDSHAKE_SIZE];
	
	if (hs) {
		if (memcmp(&hs->hash, &recv->data, HANDSHAKE_SIZE))
			return NET_ERR_HANDSHAKE_DATA_IS_NOT_HASH;
		if ((hs->step == PK_HS_INITIAL && recv->step != PK_HS_ACKNOWLEDGE) ||
			(hs->step == PK_HS_ACKNOWLEDGE && recv->step != PK_HS_FINAL))
			return NET_ERR_HANDSHAKE_BAD_SEQUENCE;
	} else if (recv->step != PK_HS_INITIAL)
		return NET_ERR_HANDSHAKE_BAD_SEQUENCE;
	
	HANDSHAKE_HASH(check, &recv->data);
	if (memcmp(&recv->hash, check, HANDSHAKE_SIZE))
		return NET_ERR_HANDSHAKE_INCORRECT_HASH;
	
	return 0;
}



#pragma mark - Handshaking

errcode send_handshake(int sockfd, int timeout) {
	errcode retv = 0;
	ssize_t bytes;
	char buf[256];
	pk_handshake_t * hs, * recvd = (pk_handshake_t *)buf;
	
	
	
	hs = make_pk_handshake(NULL);
	if ((retv = !hs))
		goto free;
	
	bytes = pk_send(sockfd, (pk_keepalive_t *)hs, 0);
	if ((retv = bytes < 0))
		goto free;
	
	
	
	bytes = pk_recv(sockfd, buf, timeout, 0);
	if ((retv = bytes < 0))
		goto free;
	
	retv = check_version((pk_keepalive_t *)recvd);
	if (retv)
		goto free;
	
	retv = check_handshake(hs, recvd);
	if (retv)
		goto free;
	
	
	
	free_packet((pk_keepalive_t *)hs);
	
	hs = make_pk_handshake(recvd);
	if ((retv = !hs))
		goto free;
	
	bytes = pk_send(sockfd, (pk_keepalive_t *)hs, 0);
	if ((retv = bytes < 0))
		goto free;
	
	
	
free:
	free_packet((pk_keepalive_t *)hs);
	return retv;
}

errcode recv_handshake(int sockfd, int timeout) {
	errcode retv = 0;
	ssize_t bytes;
	char buf[256];
	pk_handshake_t * hs = NULL, * recvd = (pk_handshake_t *)buf;
	
	
	
	bytes = pk_recv(sockfd, buf, timeout, 0);
	if ((retv = bytes < 0))
		goto free;
	
	retv = check_version((pk_keepalive_t *)recvd);
	if (retv)
		goto free;
	
	retv = check_handshake(hs, recvd);
	if (retv)
		goto free;
	
	
	
	hs = make_pk_handshake(recvd);
	if ((retv = !hs))
		goto free;
	
	bytes = pk_send(sockfd, (pk_keepalive_t *)hs, 0);
	if ((retv = bytes < 0))
		goto free;
	
	
	
	bytes = pk_recv(sockfd, buf, timeout, 0);
	if ((retv = bytes < 0))
		goto free;
	
	retv = check_version((pk_keepalive_t *)recvd);
	if (retv)
		goto free;
	
	retv = check_handshake(hs, recvd);
	if (retv)
		goto free;
	
	
	
free:
	free_packet((pk_keepalive_t *)hs);
	return retv;
}
