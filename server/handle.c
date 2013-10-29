//
//  handle.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/25/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

#define HANDLE

#include "server.h"

struct {
	enum client_type type;
	sqlite3 * db;
	sqlite3_stmt * add_stmt, * del_stmt, * list_stmt, * clear_stmt;
	struct {
		struct {
			char name[ARRLEN(FIFO_PREFIX"to.")+64/4+1];
			int fd;
		} to;
		struct {
			char name[ARRLEN(FIFO_PREFIX"to.")+64/4+1];
			int fd;
		} from;
	} pipe;
	union {
		char buf[PACKET_SIZE_MAX];
		pk_keepalive_t pk;
		pk_advertize_t ad;
		pk_forward_t fw;
	} recv;
} stt_handle;

int server_handle() {
	int retv = 0;
	
	retv = server_h_init();
	if (retv)
		goto _return;
	
	for (int rpk = 0, rpi = 0; !rpk && !rpi;) {
		rpk = server_h_packet();
		if (rpk && rpk != SEC_NO_DATA)
			retv = rpk;
		
		rpi = server_h_fifo();
		if (rpi && rpi != SEC_NO_DATA)
			retv = rpi;
		
		if (rpk == SEC_NO_DATA && rpi == SEC_NO_DATA)
			_sleep(1);
	}
	
	server_h_cleanup();
	
_return:
	return retv;
}

int server_h_init() {
	int retv, len;
	struct {
		long mtype;
		pk_miscdata_t pk;
	} * msg;
	
	close(stt_listen.sockfd);
	
	memset(&stt_handle, 0, sizeof(stt_handle));
	
	retv = (int)recv_handshake(stt_listen.acptfd, (int)cfg.timeout);
	if (retv < 0) {
		fprintf(stderr, "Bad handshake\n");
		retv = SEC_HANDSHAKE;
		goto _return;
	}
	
	len = sizeof(long) + sizeof(pk_miscdata_t) + stt_listen.addrlen;
	msg = calloc(1, len);
	if (!msg) {
		perror("calloc");
		retv = SEC_PK_MALLOC;
		goto _return;
	}
	
	msg->mtype = SQMT_EST_PIPE;
	init_pk_miscdata(&msg->pk, (const char *)&stt_listen.addr, stt_listen.addrlen);
	
	retv = msgsnd(stt_main.queue_id, msg, len, 0);
	if (retv < 0) {
		perror("msgsnd");
		retv = SEC_MSGSND;
		goto _free;
	}
	
	snprintf(stt_handle.pipe.to.name, sizeof(stt_handle.pipe.to.name), FIFO_PREFIX"to.%llux", get_sock_addr_hash((struct sockaddr *)&msg->pk.miscdata.data));
	
	retv = mkfifo(stt_handle.pipe.to.name, 0666);
	if (retv < 0) {
		perror("mkfifo");
		retv = SEC_MKFIFO;
		goto _return;
	}
	
	stt_handle.pipe.to.fd = open(stt_handle.pipe.to.name, O_RDONLY | O_NONBLOCK);
	if (stt_handle.pipe.to.fd < 0) {
		perror("open");
		retv = SEC_OPEN;
		goto _return;
	}
	
	snprintf(stt_handle.pipe.from.name, sizeof(stt_handle.pipe.from.name), FIFO_PREFIX"to.%llux", get_sock_addr_hash((struct sockaddr *)&msg->pk.miscdata.data));
	
	retv = mkfifo(stt_handle.pipe.from.name, 0666);
	if (retv < 0) {
		perror("mkfifo");
		retv = SEC_MKFIFO;
		goto _return;
	}
	
	stt_handle.pipe.from.fd = open(stt_handle.pipe.from.name, O_WRONLY);
	if (stt_handle.pipe.from.fd < 0) {
		perror("open");
		retv = SEC_OPEN;
		goto _return;
	}
	
	retv = sqlite3_open(cfg.dbname, &stt_handle.db);
	if (!stt_handle.db) {
		perror("sqlite3_open: malloc");
		retv = SEC_DB_MALLOC;
	} else if (retv) {
		fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(stt_handle.db));
		sqlite3_close(stt_handle.db);
		retv = SEC_DB_OPEN;
	}
	
_free:
	free(msg);
_return:
	return retv;
}

int server_h_fifo() {
	int retv;
	
	retv = (int)pk_recv(stt_handle.pipe.to.fd, (char *)&stt_handle.recv.buf, (int)cfg.timeout, 0);
	if (retv < 0) {
		retv = (retv == -PK_RECV_ERROR && errno == EAGAIN) ? SEC_NO_DATA : SEC_PK_RECV;
		goto _return;
	}
	
	retv = server_h_f_forward();
	
_return:
	return retv;
}

int server_h_f_forward() {
	char s[INET6_ADDRSTRLEN];
	
	inet_ntop(stt_handle.recv.fw.address.family, &stt_handle.recv.fw.address.data, s, sizeof(s));
	printf("Forward request from %s:%d\n", s, stt_handle.recv.fw.port);
	
	return 0;
}

int server_h_packet() {
	int retv, to_read;
	pk_keepalive_t * bad;
	
	retv = ioctl(stt_listen.acptfd, FIONREAD, &to_read);
	if (retv) {
		perror("ioctl");
		retv = SEC_IOCTL;
		goto _return;
	} else if (to_read < sizeof(pk_keepalive_t)) {
		retv = SEC_NO_DATA;
		goto _return;
	}
	
	retv = (int)pk_recv(stt_listen.acptfd, (char *)&stt_handle.recv.buf, (int)cfg.timeout, 0);
	if (retv < 0) {
		retv = SEC_PK_RECV;
		goto _return;
	}
	
	retv = server_h_p_version();
	if (retv)
		goto _return;
	
	if (stt_handle.recv.pk.type == PK_KEEPALIVE) {
		retv = server_h_p_keepalive();
		goto _return;
	}
	
	if (stt_handle.type == UNKNOWN || stt_handle.type == PROVIDER)
		if (stt_handle.recv.pk.type == PK_ADVERTIZE) {
			retv = server_h_p_advertize();
			goto _return;
		}
	
	if (stt_handle.type == UNKNOWN || stt_handle.type == CONSUMER)
		if (stt_handle.recv.pk.type == PK_REQUEST) {
			retv = server_h_p_request();
			goto _return;
		}
	
	if (stt_handle.recv.pk.type == PK_FORWARD) {
		retv = server_h_p_forward();
		goto _return;
	}
	
	if (stt_handle.recv.pk.type == PK_EXITING) {
		retv = server_h_p_exiting();
		goto _return;
	}
	
	bad = make_pk_keepalive(PK_BADPACKET);
	if ((retv = !bad)) {
		retv = SEC_PK_MALLOC;
		goto _return;
	}
	
	retv = (int)pk_send(stt_listen.acptfd, (pk_keepalive_t *)bad, 0);
	if (retv < 0) {
		retv = SEC_PK_SEND;
	}
	
//_free:
	free_packet(bad);
_return:
	return retv;
}

int server_h_p_version() {
	int retv;
	pk_type_t type;
	pk_keepalive_t * bad;
	
	retv = check_version(&stt_handle.recv.pk);
	
	if (retv == NET_ERR_SERVER_BAD_SOFTWARE_VERSION)
		retv = SEC_BADSWVER;
	else if (retv == NET_ERR_SERVER_BAD_NETWORK_VERSION)
		retv = SEC_BADNETVER;
	else if (!retv)
		goto _return;
	
	switch (retv) {
		case NET_ERR_BAD_MAJOR_VERSION:
		case NET_ERR_BAD_MINOR_VERSION:
		case NET_ERR_BAD_REVISION:
		case NET_ERR_BAD_SUBREVISION:
			type = PK_BADSWVER;
			break;
		case NET_ERR_BAD_NETWORK_VERSION:
			type = PK_BADNETVER;
			break;
		default:
			retv = SEC_UNKNOWN;
			goto _return;
			break;
	}
	
	bad  = make_pk_keepalive(type);
	if ((retv = !bad)) {
		retv = SEC_PK_MALLOC;
		goto _return;
	}
	
	pk_send(stt_listen.acptfd, bad, 0);
	free_packet(bad);
	
	retv = SEC_SENDBADVER;
	
_return:
	return retv;
}

int server_h_p_keepalive() {
	return 0;
}

int server_h_p_advertize() {
	int retv;
	
	if (stt_handle.type != PROVIDER) {
		retv = server_h_p_ad_init();
		if (retv)
			goto _return;
	}
	
	// if the name is length 1 and the only byte is -, remove the service
	if (stt_handle.recv.ad.name.length == 1 && stt_handle.recv.ad.name.data[0] == '-') {
		retv = server_h_p_ad_remove();
		goto _return;
	}
	
	const char * name = NULL;
	
	if (stt_handle.recv.ad.name.length == 1 && stt_handle.recv.ad.name.data[0] == '+')
		name = get_port_service_name(stt_handle.recv.ad.port, NULL);
	else
		name = (char *)&stt_handle.recv.ad.name.data;
	
	if (name)
		printf("Provider advertizing %s on %d\n", name, stt_handle.recv.ad.port);
	else
		printf("Provider advertizing %d\n", stt_handle.recv.ad.port);
	
	if (name)
		retv = sqlite3_bind_text(stt_handle.add_stmt, 1, name, -1, SQLITE_TRANSIENT);
	else
		retv = sqlite3_bind_null(stt_handle.add_stmt, 1);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	
	retv = sqlite3_bind_int(stt_handle.add_stmt, 2, stt_handle.recv.ad.port);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	
	retv = sqlite3_bind_int(stt_handle.add_stmt, 3, stt_listen.addr.ss_family);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	
	sqlite3_step(stt_handle.add_stmt);
	
	retv = sqlite3_reset(stt_handle.add_stmt);
	if (retv) {
		retv = SEC_DB_RESET;
		goto _eprint;
	}
	
_return:
	return retv;
_eprint:
	fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(stt_handle.db));
	goto _return;
}

int server_h_p_ad_remove() {
	int retv;
	
	printf("Service no longer accessible on port %d of provider\n", stt_handle.recv.ad.port);
	
	retv = sqlite3_bind_int(stt_handle.del_stmt, 1, stt_handle.recv.ad.port);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	
	sqlite3_step(stt_handle.del_stmt);
	
	retv = sqlite3_reset(stt_handle.del_stmt);
	if (retv) {
		retv = SEC_DB_RESET;
		goto _eprint;
	}
	
_return:
	return retv;
_eprint:
	fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(stt_handle.db));
	goto _return;
}

int server_h_p_ad_init() {
	int retv;
	
	stt_handle.type = PROVIDER;
	
	retv = sqlite3_prepare_v2(stt_handle.db, INSERT_STMT_STR, -1, &stt_handle.add_stmt,   NULL);
	if (retv) {
		retv = SEC_DB_PREPARE;
		goto _eprint;
	}
	retv = sqlite3_prepare_v2(stt_handle.db, DELETE_STMT_STR, -1, &stt_handle.del_stmt,   NULL);
	if (retv) {
		retv = SEC_DB_PREPARE;
		goto _eprint;
	}
	retv = sqlite3_prepare_v2(stt_handle.db, CLEAR_STMT_STR,  -1, &stt_handle.clear_stmt, NULL);
	if (retv) {
		retv = SEC_DB_PREPARE;
		goto _eprint;
	}
	
	struct sockaddr * sa = (struct sockaddr *)&stt_listen.addr;
	
	retv = sqlite3_bind_blob(stt_handle.add_stmt, 4, &sa->sa_data, stt_listen.addrlen, SQLITE_TRANSIENT);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	retv = sqlite3_bind_blob(stt_handle.del_stmt, 2, &sa->sa_data, stt_listen.addrlen, SQLITE_TRANSIENT);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	retv = sqlite3_bind_blob(stt_handle.clear_stmt, 1, &sa->sa_data, stt_listen.addrlen, SQLITE_TRANSIENT);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	
_return:
	return retv;
_eprint:
	fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(stt_handle.db));
	goto _return;
}

int server_h_p_request() {
	int retv;
	pk_service_t * serv;
	
	if (stt_handle.type != CONSUMER) {
		retv = server_h_p_rq_init();
		if (retv)
			goto _return;
	}
	
	printf("Client requesting services\n");
	
	serv = (pk_service_t *)alloc_packet(PACKET_SIZE_MAX);
	if ((retv = !serv)) {
		retv = SEC_PK_MALLOC;
		goto _return;
	}
	
	while ((retv = sqlite3_step(stt_handle.list_stmt)) == SQLITE_ROW) {
		const char * name = (const char *)sqlite3_column_text(stt_handle.list_stmt, 0);
		int port = sqlite3_column_int(stt_handle.list_stmt, 1);
		sa_family_t family = sqlite3_column_int(stt_handle.list_stmt, 2);
		const void * address = sqlite3_column_blob(stt_handle.list_stmt, 3);
		int length = sqlite3_column_int(stt_handle.list_stmt, 4);
		
		init_pk_service(serv, NULL, port, name);
		serv->address.family = family;
		memcpy(serv->address.data, address, length);
		
		retv = (int)pk_send(stt_listen.acptfd, (pk_keepalive_t *)serv, 0);
		if (retv < 0) {
			retv = SEC_PK_SEND;
			goto _free;
		}
	}
	retv = sqlite3_reset(stt_handle.add_stmt);
	if (retv) {
		retv = SEC_DB_RESET;
		goto _free;
	}
	
	serv->_super.type = PK_RESPONSE;
	serv->_super.size = sizeof(pk_keepalive_t);
	
	retv = (int)pk_send(stt_listen.acptfd, (pk_keepalive_t *)serv, 0);
	if (retv < 0)
		retv = SEC_PK_SEND;
	
_free:
	free_packet((pk_keepalive_t *)serv);
_return:
	return retv;
}

int server_h_p_rq_init() {
	int retv;
	
	stt_handle.type = CONSUMER;
	
	retv = sqlite3_prepare_v2(stt_handle.db, SELECT_STMT_STR, -1, &stt_handle.list_stmt, NULL);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	
_return:
	return retv;
_eprint:
	fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(stt_handle.db));
	goto _return;
}

int server_h_p_forward() {
	int retv;
	
	printf("Client requesting packet forwarding\n");
	retv = (int)pk_send(stt_handle.pipe.from.fd, &stt_handle.recv.pk, 0);
	
//_return:
	return retv;
}

int server_h_p_exiting() {
	int retv;
	
	printf("Client/provider exiting\n");
	retv = SEC_PK_EXITING;
	
//_return:
	return retv;
}

int server_h_cleanup() {
	if (stt_handle.type == PROVIDER) {
		printf("Provider disconnected, removing services\n");
		sqlite3_step(stt_handle.clear_stmt);
		sqlite3_reset(stt_handle.clear_stmt);
	} else if (stt_handle.type == CONSUMER) {
		printf("Client disconnected\n");
	}
	
	sqlite3_finalize(stt_handle.clear_stmt);
	sqlite3_finalize(stt_handle.list_stmt);
	sqlite3_finalize(stt_handle.del_stmt);
	sqlite3_finalize(stt_handle.add_stmt);
	sqlite3_close(stt_handle.db);
	
	return 0;
}