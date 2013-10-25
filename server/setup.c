//
//  setup.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/25/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <sqlite3.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "server.h"

static const char server_db_tmp[] = SERVER_DB_TMP;

int server_configure() {
	int retv;
	
	struct config_var vars[] = {
		{"var-setup",		{ct_var, ct_req, ct_str},	'0',	NULL,						NULL,						&cfg.var_setup	},
		{"verbose",			{ct_flg, 0     , 0     },	'v',	NULL,						(void *)1,					&cfg.verbose	},
		{"quiet",			{ct_flg, 0     , 0     },	'q',	NULL,						(void *)-1,					&cfg.verbose	},
		{"database",		{ct_var, ct_req, ct_str},	'd',	ENV_PREFIX"DATABASE",		(void *)server_db_tmp,		&cfg.dbname		},
		{"listen",			{ct_var, ct_req, ct_str},	'l',	ENV_PREFIX"LISTEN",			"localhost",				&cfg.listen		},
		{"port",			{ct_var, ct_req, ct_str},	'p',	ENV_PREFIX"PORT",			SERVER_PORT,				&cfg.port		},
		{"packet-backlog",	{ct_var, ct_req, ct_str},	'b',	ENV_PREFIX"PACKET_BACKLOG",	(void *)10,					&cfg.backlog	},
		{"packet-timeout",	{ct_var, ct_req, ct_num},	't',	ENV_PREFIX"PACKET_TIMEOUT",	(void *)DEFAULT_TIMEOUT,	&cfg.timeout	}
	};
	
	retv = config(stt_main.argc, stt_main.argv, ARRLEN(vars), vars);
	if (retv) {
		retv = SEC_OPTIONS;
		goto _return;
	}
	
	if (cfg.var_setup) {
		if ((retv = !strcasecmp("prefix", cfg.var_setup))) {
			printf(ENV_PREFIX"\n");
		} else if ((retv = !strcasecmp("list", cfg.var_setup))) {
			for (int i = 0; i < ARRLEN(vars); i++)
				if (vars[i].env_name)
					printf("%s\n", vars[i].env_name + ENV_PREFIX_LEN - 1);
		} else {
			fprintf(stderr, "Unrecognized var-setup option: %s\n", cfg.var_setup);
		}
		retv = SEC_VAR_SETUP;
	}
	
_return:
	return retv;
}

int server_init_db() {
	int retv;
	sqlite3 * db;
	char * retname;
	
	retname = mktemp(cfg.dbname);
	if((retv = !retname))
	{
		perror("mktemp");
		retv = SEC_MKTEMP;
		goto _return;
	}
	
	retv = sqlite3_open(cfg.dbname, &db);
	if (!db) {
		perror("sqlite3_open: malloc");
		retv = SEC_DB_MALLOC;
		goto _return;
	} else if (retv) {
		fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		retv = SEC_DB_OPEN;
		goto _return;
	}
	
	retv = sqlite3_exec(db, CREATE_STMT_STR, NULL, NULL, NULL);
	if (retv) {
		fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db));
		retv = SEC_DB_CREATE;
	}
	
	sqlite3_close(db);
	
_return:
	return retv;
}

int server_init_sig() {
	int retv;
    struct sigaction sa;
	
    sa.sa_handler = _waitpid_sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
	
	retv = sigaction(SIGCHLD, &sa, NULL);
    if (retv < 0) {
        perror("sigaction");
		retv = SEC_SIGACTION;
    }
	
//_return:
	return retv;
}