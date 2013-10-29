//
//  forward.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/26/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <errno.h>

#define FORWARD

#include "server.h"
#include "uthash.h"

struct child_key {
	in_port_t port;
	union {
		struct in_addr in;
		struct in6_addr in6;
	} address;
};

struct child_info {
	UT_hash_handle hh;
	struct child_key key;
	enum client_type type;
	struct {
		int to, from;
	} pipe;
};

int server_f_getkey_sockaddr(struct sockaddr * sa, struct child_key * key);
int server_f_getkey_pkaddr(struct _pk_address * pa, uint16_t port, struct child_key * key);

#define HASH_ADD_CI(head, ci) HASH_ADD(hh, head, key, sizeof(struct child_key), ci)
#define HASH_FIND_CI(head, keyptr, out) HASH_FIND(hh, head, keyptr, sizeof(struct child_key), out)

struct {
	struct child_info * head;
} stt_forward;

int server_forward() {
	int retv = 0;
	union {
		char buf[PACKET_SIZE_MAX];
		pk_forward_t pk;
	} recv;
	struct child_key key;
	
	memset(&stt_forward, 0, sizeof(stt_forward));
	
	for (int rcvd = 0; !retv; rcvd = 0) {
		retv = server_f_msgrcv();
		if (!retv)
			rcvd = 1;
		else if (retv != SEC_NO_DATA)
			goto _clear;
		
		for (struct child_info * tgt, * child = stt_forward.head; child; child = child->hh.next)
			if (child->pipe.from) {
				retv = (int)pk_recv(child->pipe.from, (char *)&recv.buf, (int)cfg.timeout, 0);
				if (retv > 0)
					rcvd = 1;
				else {
					retv = SEC_PK_RECV;
					goto _clear;
				}
				
				server_f_getkey_pkaddr(&recv.pk.address, recv.pk.port, &key);
				HASH_FIND_CI(stt_forward.head, &key, tgt);
				// if there's an error, change the type and send it back
				if (!tgt) {
					recv.pk._super.type = PK_BADADDR;
					tgt = child;
				} else if (!tgt->pipe.to) {
					recv.pk._super.type = PK_FWNOTRDY;
					tgt = child;
				}
				
				retv = (int)pk_send(tgt->pipe.to, (pk_keepalive_t *)&recv.pk, 0);
				if (retv < 0) {
					retv = SEC_PK_SEND;
					goto _clear;
				}
			}
	}
	
	struct child_info * tmp, * tgt;
_clear:
	HASH_ITER(hh, stt_forward.head, tgt, tmp) {
		free(tgt);
	}
_return:
	return retv;
}

int server_f_msgrcv() {
	int retv;
	struct {
		long mtype;
		union {
			char buf[PACKET_SIZE_MAX];
			pk_miscdata_t pk;
		} recv;
	} msg;
	struct child_info * child;
	struct child_key key;
	char fifo[ARRLEN(FIFO_PREFIX"from.")+64/4+1];
	
	retv = (int)msgrcv(stt_main.queue_id, &msg, sizeof(msg), SQMT_ALL, IPC_NOWAIT);
	if (retv < 0) {
		if (errno == ENOMSG)
			retv = SEC_NO_DATA;
		else {
			perror("msgrcv");
			retv = SEC_MSGRCV;
		}
		goto _return;
	}
	
	switch (msg.mtype) {
		case SQMT_NEW_CONN:
			child = calloc(1, sizeof(struct child_info));
			if (!child) {
				perror("calloc");
				retv = SEC_PK_MALLOC;
				goto _return;
			}
			
			server_f_getkey_sockaddr((struct sockaddr *)&msg.recv.pk.miscdata.data, &child->key);
			HASH_ADD_CI(stt_forward.head, child);
			break;
			
		case SQMT_EST_PIPE:
			server_f_getkey_sockaddr((struct sockaddr *)&msg.recv.pk.miscdata.data, &key);
			HASH_FIND_CI(stt_forward.head, &key, child);
			
			if (!child) {
				child = calloc(1, sizeof(struct child_info));
				if (!child) {
					perror("calloc");
					retv = SEC_PK_MALLOC;
					goto _return;
				}
				
				server_f_getkey_sockaddr((struct sockaddr *)&msg.recv.pk.miscdata.data, &child->key);
				HASH_ADD_CI(stt_forward.head, child);
			}
			
			snprintf(fifo, sizeof(fifo), FIFO_PREFIX"to.%llux", get_sock_addr_hash((struct sockaddr *)&msg.recv.pk.miscdata.data));
			child->pipe.to = open(fifo, O_WRONLY);
			if (child->pipe.to < 0) {
				perror("open");
				retv = SEC_OPEN;
				goto _return;
			}
			
			snprintf(fifo, sizeof(fifo), FIFO_PREFIX"from.%llux", get_sock_addr_hash((struct sockaddr *)&msg.recv.pk.miscdata.data));
			child->pipe.from = open(fifo, O_RDONLY | O_NONBLOCK);
			if (child->pipe.from < 0) {
				perror("open");
				retv = SEC_OPEN;
				goto _return;
			}
			
		default:
			break;
	}
	
_return:
	return retv;
}

int server_f_getkey_sockaddr(struct sockaddr * sa, struct child_key * key) {
	key->port = get_in_port(sa);
	memcpy(get_in_addr(sa), &key->address, get_in_addr_len(sa));
	
	return 0;
}

int server_f_getkey_pkaddr(struct _pk_address * pa, uint16_t port, struct child_key * key) {
	key->port = port;
	memcpy(&pa->data, &key->address, pa->family == AF_INET ? sizeof(struct in_addr) : sizeof(struct in6_addr));
	
	return 0;
}