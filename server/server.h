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

#define MESSAGE_QUEUE_NAME	"/com.lensflare.pwnat.server"
#define FIFO_PREFIX			"/var/tmp/pwnat.fifo."

#define CREATE_STMT_STR "CREATE TABLE Services (Name VARCHAR2(220) NULL, Port INT NON NULL, Family INT NON NULL, Address BLOB NON NULL)"
#define CLEAR_STMT_STR "DELETE FROM Services WHERE Address = ?"
#define INSERT_STMT_STR	"INSERT INTO Services (Name, Port, Family, Address) VALUES (?, ?, ?, ?)"
#define DELETE_STMT_STR	"DELETE FROM Services WHERE Port = ? AND Address = ?"
#define SELECT_STMT_STR	"SELECT Name, Port, Family, Address, LENGTH(Address) as Length FROM Services"

enum server_queue_message_type {
	SQMT_ALL,
	SQMT_NEW_CONN,
	SQMT_EST_PIPE
};

enum server_error_code {
	SEC_UNKNOWN,
	SEC_OPTIONS,
	SEC_USAGE,
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
	SEC_DB_RESET,
	SEC_IOCTL,
	SEC_NO_DATA,
	SEC_PIPE,
	SEC_WRITE,
	SEC_FTOK,
	SEC_MSGGET,
	SEC_MSGRCV,
	SEC_MSGSND,
	SEC_MSGCTL,
	SEC_BADDATA,
	SEC_BADFW,
	SEC_OPEN,
	SEC_MKFIFO
};

#pragma mark - Configuration

#ifndef MAIN
extern struct {
	cfgint help, verbose, backlog, timeout;
	char * var_setup, * listen, * port, * dbname;
} cfg;
#endif


#pragma mark - State

#ifndef MAIN
extern struct {
	int argc;
	const char ** argv;
	pid_t pid;
	key_t queue_key;
	int queue_id;
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

//  server_setup
int server_s_config();
int server_s_ipc();
int server_s_sqlite();
int server_s_sigaction();

#pragma mark - Listen

int server_listen();
int server_l_accept();
int server_l_a_writedata();

#pragma mark - Handle

enum client_type {
	UNKNOWN,
	PROVIDER,
	CONSUMER
};

int server_handle();
int server_h_init();
int server_h_fifo();
int server_h_f_forward();
int server_h_packet();
int server_h_p_version();
int server_h_p_keepalive();
int server_h_p_advertize();
int server_h_p_ad_init();
int server_h_p_ad_remove();
int server_h_p_request();
int server_h_p_rq_init();
int server_h_p_forward();
int server_h_p_exiting();
int server_h_cleanup();

#pragma mark - Forward

int server_forward();
int server_f_msgrcv();

#endif
