//
//  main.c
//  server
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>

#define MAIN

#include "server.h"

struct {
	cfgint verbose, backlog, timeout;
	char * var_setup, * listen, * port, * dbname;
} cfg;

struct {
	int argc;
	const char ** argv;
} stt_main;

int main(int argc, const char * argv[]) {
	int retv;
	
	retv = server_configure();
	if (retv) {
		if (retv == SEC_OPTIONS)
			usage();
		goto _return;
	}
	
	char asdf[] = "/tmp/pwnat.db.XXXl";
	cfg.dbname = asdf;
	
	retv = server_init_db();
	if (retv == SEC_DB_MALLOC)
		goto _return;
	else if (retv)
		goto _unlink;
	
	retv = server_init_sig();
	if (retv)
		goto _unlink;
	
	server_listen();
	
_unlink:
	unlink(cfg.dbname);
_return:
	return retv;
}

void usage() {
	printf("Usage: %s [-0 prefix | list] [-q | -v] [-d database] [-l listen] [-p port] [-t timeout]\n", stt_main.argv[0]);
}
