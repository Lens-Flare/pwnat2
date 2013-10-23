//
//  main.c
//  server
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>

#include "../common/network.h"



#pragma mark Prototypes

int fork_listener(const char * port, int backlog, pid_t * cpid, const char * dbname);
int do_listener(int sockfd, const char * dbname);
int fork_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd, const char * dbname);
int do_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd, const char * dbname);



#pragma mark - SQL Statements

#define CREATE_STMT_STR "CREATE TABLE Services (Name VARCHAR2(220) NULL, Port INT NON NULL, Family INT NON NULL, Address BLOB NON NULL)"
#define CLEAR_STMT_STR "DELETE FROM Services WHERE Address = ?"
#define INSERT_STMT_STR	"INSERT INTO Services (Name, Port, Family, Address) VALUES (?, ?, ?, ?)"
#define DELETE_STMT_STR	"DELETE FROM Services WHERE Port = ? AND Address = ?"
#define SELECT_STMT_STR	"SELECT Name, Port, Family, Address, LENGTH(Address) as Length FROM Services"



#pragma mark - Main

int main(int argc, const char * argv[]) {
	int retv = 0;
	char dbname[L_tmpnam];
	sqlite3 * db = NULL;
	
	tmpnam(dbname);
	printf("opening a new database at %s\n", dbname);
	
	retv = sqlite3_open(dbname, &db);
	if (!db) {
		perror("sqlite3_open: malloc");
		goto exit;
	} else if (retv) {
		fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		goto exit;
	}
	
	retv = sqlite3_exec(db, CREATE_STMT_STR, NULL, NULL, NULL);
	if (retv) {
		fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		goto exit;
	}
	
	sqlite3_close(db);
	
    struct sigaction sa;
    sa.sa_handler = _waitpid_sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
	
	fork_listener(SERVER_PORT, 10, NULL, dbname);
	
exit:
	unlink(dbname);
	return retv;
}



#pragma mark - Connection Listener

int fork_listener(const char * port, int backlog, pid_t * cpid, const char * dbname) {
	int sockfd;
	
	if (listen_socket("localhost", (char *)port, backlog, &sockfd)) {
		return 1;
	}
	
	if (cpid == NULL || !(*cpid = fork()))
		exit(do_listener(sockfd, dbname));
	
	return 0;
}
int do_listener(int sockfd, const char * dbname) {
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int acptfd;
    struct sigaction sa;
	
    sa.sa_handler = _waitpid_sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
	
	while (1) {
		if ((acptfd = accept(sockfd, (struct sockaddr *)&addr, &addrlen)) < 0) {
			perror("accept");
			continue;
		}
		
		fork_handler(sockfd, addr, addrlen, acptfd, dbname);
		close(acptfd);
	}
	
	return 0;
}



#pragma mark - Connection Handler

int fork_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd, const char * dbname) {
//	if (!fork())
		exit(do_handler(sockfd, addr, addrlen, acptfd, dbname));
	return 0;
}
int do_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd, const char * dbname) {
	ssize_t retv = 0;
	char buf[PACKET_SIZE_MAX];
	unsigned char is_provider = 0;
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	sqlite3 * db = NULL;
	sqlite3_stmt * add_stmt = NULL, * del_stmt = NULL, * list_stmt = NULL, * clear_stmt = NULL;
	
	close(sockfd);
	
	if ((retv = recv_handshake(acptfd))) {
		fprintf(stderr, "Bad handshake\n");
		goto close;
	}
	
	retv = sqlite3_open(dbname, &db);
	if (!db) {
		perror("sqlite3_open: malloc");
		goto close;
	} else if (retv)
		goto sql_fail;
	
	while (1) {
		pk_keepalive_t * bad;
		
		if ((retv = pk_recv(acptfd, buf, 0) < 0))
			goto clear;
		
		if ((retv = check_version(pk))) {
			if (retv != NET_ERR_SERVER_BAD_SOFTWARE_VERSION && retv != NET_ERR_SERVER_BAD_NETWORK_VERSION) {
				pk_type_t type;
				
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
						goto clear;
						break;
				}
				
				if ((retv = !(bad = make_pk_keepalive(type))))
					goto clear;
				
				pk_send(acptfd, bad, 0);
				free_packet(bad);
			}
			goto close;
		}
		
		switch (pk->type) {
			case PK_KEEPALIVE:
				break;
				
			case PK_ADVERTIZE:
				if (!is_provider) {
					retv = sqlite3_prepare_v2(db, INSERT_STMT_STR, -1, &add_stmt,   NULL); if (retv) goto sql_fail;
					retv = sqlite3_prepare_v2(db, DELETE_STMT_STR, -1, &del_stmt,   NULL); if (retv) goto sql_fail;
					retv = sqlite3_prepare_v2(db, CLEAR_STMT_STR,  -1, &clear_stmt, NULL); if (retv) goto sql_fail;
					
					retv = sqlite3_bind_address(add_stmt, 4, (struct sockaddr *)&addr); if (retv) goto sql_fail;
					retv = sqlite3_bind_address(del_stmt, 2, (struct sockaddr *)&addr); if (retv) goto sql_fail;
					retv = sqlite3_bind_address(clear_stmt, 1, (struct sockaddr *)&addr); if (retv) goto sql_fail;
				}
				
				is_provider = 1;
				pk_advertize_t * ad = (pk_advertize_t *)pk;
				
				// if the name is length 1 and the only byte is -, remove the service
				if (ad->name.length == 1 && ad->name.data[0] == '-') {
					printf("Service no longer accessible on port %d of provider\n", ad->port);
					retv = sqlite3_bind_int(del_stmt, 1, ad->port); if (retv) goto clear;
					retv = sqlite3_step(del_stmt);
					retv = sqlite3_reset(del_stmt); if (retv) goto clear;
					retv = sqlite3_clear_bindings(del_stmt); if (retv) goto clear;
					break;
				}
				
				const char * name = NULL;
				
				if (ad->name.length == 1 && ad->name.data[0] == '+')
					name = get_port_service_name(ad->port, NULL);
				else
					name = (char *)&ad->name.data;
				
				if (name)
					printf("Provider advertizing %s on %d\n", name, ad->port);
				else
					printf("Provider advertizing %d\n", ad->port);
				
				if (name)
					retv = sqlite3_bind_text(add_stmt, 1, name, -1, SQLITE_TRANSIENT);
				else
					retv = sqlite3_bind_null(add_stmt, 1);
				if (retv) goto sql_fail;
				
				retv = sqlite3_bind_int(add_stmt, 2, ad->port); if (retv) goto clear;
				retv = sqlite3_bind_int(add_stmt, 3, addr.ss_family);
				retv = sqlite3_step(add_stmt);
				retv = sqlite3_reset(add_stmt); if (retv) goto clear;
				
				break;
				
			case PK_REQUEST:
				if (!list_stmt) {
					retv = sqlite3_prepare_v2(db, SELECT_STMT_STR, -1, &list_stmt, NULL); if (retv) goto sql_fail;
				}
				
				printf("Client requesting services\n");
				
				pk_service_t * serv = (pk_service_t *)alloc_packet(PACKET_SIZE_MAX); if (!serv) goto clear;
				
				while ((retv = sqlite3_step(list_stmt)) == SQLITE_ROW) {
					const char * name = (const char *)sqlite3_column_text(list_stmt, 0);
					int port = sqlite3_column_int(list_stmt, 1);
					sa_family_t family = sqlite3_column_int(list_stmt, 2);
					const void * address = sqlite3_column_blob(list_stmt, 3);
					int length = sqlite3_column_int(list_stmt, 4);
					
					init_pk_service(serv, NULL, port, name);
					serv->address.family = family;
					for (int i = 0; i < 4 && i < length; i++)
						serv->address.data[i] = ((uint32_t *)address)[i];
					
					retv = pk_send(acptfd, (pk_keepalive_t *)serv, 0);
					if (retv < 0) {
						free_packet((pk_keepalive_t *)serv);
						goto clear;
					}
				}
				retv = sqlite3_reset(add_stmt); if (retv) goto clear;
				
				free_packet((pk_keepalive_t *)serv);
				
				pk_keepalive_t * pk = make_pk_keepalive(PK_RESPONSE);
				if (!pk)
					goto clear;
				retv = pk_send(acptfd, pk, 0);
				free_packet(pk);
				if (retv < 0) {
					goto clear;
				}
				
				break;
				
			case PK_FORWARD:
				printf("Client requesting packet forwarding\n");
				// forward packet
				break;
				
			case PK_EXITING:
				printf("Client/provider exiting\n");
				goto close;
				
			default:
				if ((retv = !(bad = make_pk_keepalive(PK_BADPACKET))))
					goto clear;
				retv = pk_send(acptfd, bad, 0);
				if (retv < 0) {
					free_packet(bad);
					goto clear;
				}
				
				break;
		}
	}
	
clear:
	if (is_provider) {
		printf("Provider disconnected, removing services\n");
		sqlite3_step(clear_stmt);
		sqlite3_reset(clear_stmt);
	}
close:
	sqlite3_finalize(clear_stmt);
	sqlite3_finalize(list_stmt);
	sqlite3_finalize(del_stmt);
	sqlite3_finalize(add_stmt);
	sqlite3_close(db);
	close(acptfd);
	return (int)retv;
sql_fail:
	fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db));
	goto clear;
}