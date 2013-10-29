//
//  main.c
//  server
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

#define MAIN

#include "server.h"

struct {
	cfgint help, verbose, backlog, timeout;
	char * var_setup, * listen, * port, * dbname;
} cfg;

struct {
	int argc;
	const char ** argv;
	pid_t pid;
	key_t queue_key;
	int queue_id;
} stt_main;

int main(int argc, const char * argv[]) {
	int retv;
	
	memset(&stt_main, 0, sizeof(stt_main));
	
	stt_main.argc = argc;
	stt_main.argv = argv;
	
	stt_main.pid = getpid();
	
	retv = server_s_config();
	if (retv) {
		if (retv == SEC_OPTIONS || retv == SEC_USAGE)
			usage();
		goto _return;
	}
	
	retv = server_s_ipc();
	if (retv)
		goto _return;
	
	retv = server_s_sqlite();
	if (retv == SEC_DB_MALLOC)
		goto _rmqueue;
	else if (retv)
		goto _unlink;
	
	retv = server_s_sigaction();
	if (retv)
		goto _unlink;
	
	server_listen();
	
_unlink:
	unlink(cfg.dbname);
_rmqueue:
	msgctl(stt_main.queue_id, IPC_RMID, NULL);
_return:
	return retv;
}

void usage() {
	printf("Usage: %s [-0 prefix | list] [-q | -v] [-d database] [-l listen] [-p port] [-t timeout]\n", stt_main.argv[0]);
}
