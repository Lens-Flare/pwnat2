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

#include "server.h"

#define UNKNOWN		0
#define PROVIDER	1
#define CONSUMER	2

struct {
	int client_type;
	sqlite3 * db;
	sqlite3_stmt * add_stmt, * del_stmt, * list_stmt, * clear_stmt;
	union {
		char buf[PACKET_SIZE_MAX];
		pk_keepalive_t pk;
		pk_advertize_t ad;
	} recv;
} stt_handle;

int server_handle() {
	int retv = 0;
	
	retv = server_handler_init();
	if (retv)
		goto _return;
	
	for (retv = 0; !retv; retv = server_packet());
	
	server_con_cleanup();
	
_return:
	return retv;
}

int server_handler_init() {
	int retv;
	
	close(stt_listen.sockfd);
	
	memset(&stt_handle, 0, sizeof(stt_handle));
	
	retv = (int)recv_handshake(stt_listen.acptfd, (int)cfg.timeout);
	if (retv < 0) {
		fprintf(stderr, "Bad handshake\n");
		retv = SEC_HANDSHAKE;
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
	
_return:
	return retv;
}

int server_packet() {
	int retv;
	pk_keepalive_t * bad;
	
	retv = (int)pk_recv(stt_listen.acptfd, (char *)&stt_handle.recv.buf, (int)cfg.timeout, 0);
	if (retv < 0) {
		retv = SEC_PK_RECV;
		goto _return;
	}
	
	retv = server_version();
	if (retv)
		goto _return;
	
	if (stt_handle.recv.pk.type == PK_KEEPALIVE) {
		retv = server_keepalive();
		goto _return;
	}
	
	if (stt_handle.client_type == UNKNOWN || stt_handle.client_type == PROVIDER)
		if (stt_handle.recv.pk.type == PK_ADVERTIZE) {
			retv = server_advertize();
			goto _return;
		}
	
	if (stt_handle.client_type == UNKNOWN || stt_handle.client_type == CONSUMER)
		if (stt_handle.recv.pk.type == PK_REQUEST) {
			retv = server_request();
			goto _return;
		}
	
	if (stt_handle.recv.pk.type == PK_FORWARD) {
		retv = server_forward();
		goto _return;
	}
	
	if (stt_handle.recv.pk.type == PK_EXITING) {
		retv = server_exiting();
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

int server_version() {
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

int server_keepalive() {
	return 0;
}

int server_advertize() {
	int retv;
	
	if (stt_handle.client_type != PROVIDER) {
		retv = server_ad_init();
		if (retv)
			goto _return;
	}
	
	// if the name is length 1 and the only byte is -, remove the service
	if (stt_handle.recv.ad.name.length == 1 && stt_handle.recv.ad.name.data[0] == '-') {
		retv = server_remove();
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

int server_remove() {
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

int server_ad_init() {
	int retv;
	
	stt_handle.client_type = PROVIDER;
	
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
	
	retv = sqlite3_bind_address(stt_handle.add_stmt,   4, (struct sockaddr *)&stt_listen.addr);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	retv = sqlite3_bind_address(stt_handle.del_stmt,   2, (struct sockaddr *)&stt_listen.addr);
	if (retv) {
		retv = SEC_DB_BIND;
		goto _eprint;
	}
	retv = sqlite3_bind_address(stt_handle.clear_stmt, 1, (struct sockaddr *)&stt_listen.addr);
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

int server_request() {
	int retv;
	pk_service_t * serv;
	
	if (stt_handle.client_type != CONSUMER) {
		retv = server_rq_init();
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

int server_rq_init() {
	int retv;
	
	stt_handle.client_type = CONSUMER;
	
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

int server_forward() {
	int retv;
	
	printf("Client requesting packet forwarding\n");
	retv = SEC_UNKNOWN;
	
//_return:
	return retv;
}

int server_exiting() {
	int retv;
	
	printf("Client/provider exiting\n");
	retv = SEC_PK_EXITING;
	
//_return:
	return retv;
}

void server_con_cleanup() {
	if (stt_handle.client_type == PROVIDER) {
		printf("Provider disconnected, removing services\n");
		sqlite3_step(stt_handle.clear_stmt);
		sqlite3_reset(stt_handle.clear_stmt);
	} else if (stt_handle.client_type == CONSUMER) {
		printf("Client disconnected\n");
	}
	
	sqlite3_finalize(stt_handle.clear_stmt);
	sqlite3_finalize(stt_handle.list_stmt);
	sqlite3_finalize(stt_handle.del_stmt);
	sqlite3_finalize(stt_handle.add_stmt);
	sqlite3_close(stt_handle.db);
}