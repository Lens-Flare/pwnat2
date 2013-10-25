//
//  server.h
//  pwnat2
//
//  Created by Ethan Reesor on 10/25/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#ifndef pwnat2_server_h
#define pwnat2_server_h

#include "../common/common.h"

#pragma mark - Definitions

#define CREATE_STMT_STR "CREATE TABLE Services (Name VARCHAR2(220) NULL, Port INT NON NULL, Family INT NON NULL, Address BLOB NON NULL)"
#define CLEAR_STMT_STR "DELETE FROM Services WHERE Address = ?"
#define INSERT_STMT_STR	"INSERT INTO Services (Name, Port, Family, Address) VALUES (?, ?, ?, ?)"
#define DELETE_STMT_STR	"DELETE FROM Services WHERE Port = ? AND Address = ?"
#define SELECT_STMT_STR	"SELECT Name, Port, Family, Address, LENGTH(Address) as Length FROM Services"



#pragma mark - Errors

enum server_error_code {
	SEC_UNKNOWN,
	SEC_OPTIONS,
	SEC_VAR_SETUP,
	SEC_MKTEMP,
	SEC_DB_MALLOC,
	SEC_DB_OPEN,
	SEC_DB_CREATE,
	SEC_SIGACTION,
	SEC_LISTEN,
	SEC_ACCEPT,
	SEC_HANDSHAKE,
	SEC_PK_RECV,
	SEC_PK_SEND,
	SEC_PK_MALLOC,
	SEC_PK_EXITING,
	SEC_BADSWVER,
	SEC_BADNETVER,
	SEC_SENDBADVER,
	SEC_DB_PREPARE,
	SEC_DB_BIND,
	SEC_DB_RESET
};



#pragma mark - Configuration

#ifndef MAIN
extern struct {
	cfgint verbose, backlog, timeout;
	char * var_setup, * listen, * port, * dbname;
} cfg;
#endif


#pragma mark - State

#ifndef MAIN
extern struct {
	int argc;
	const char ** argv;
} stt_main;
#endif

#ifndef LISTEN
extern struct {
	int sockfd, acptfd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
} stt_listen;
#endif

#pragma mark - Main

void usage();

#pragma mark - Setup

int server_configure();
int server_init_db();
int server_init_sig();

#pragma mark - Listen

int server_listen();
int server_accept();

#pragma mark - Handle

int server_handle();
int server_handler_init();
int server_packet();
int server_version();
int server_keepalive();
int server_advertize();
int server_ad_init();
int server_remove();
int server_request();
int server_rq_init();
int server_forward();
int server_exiting();
void server_con_cleanup();

#endif
