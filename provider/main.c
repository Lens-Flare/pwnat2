//
//  main.c
//  provider
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
#include <getopt.h>
#include <limits.h>
#include <signal.h>

#include "../common/network.h"
#include "../common/common.h"

int do_keepalive(void * param);

struct keepalive_param {
	int * sockfd, * interval;
};

//struct config_var {
//	char * name;			// long option
//	struct {
//		uint8_t res:5;
//		uint8_t flag:1;		// true if this option is a flag - implies numeric and no argument
//		uint8_t required:1;	// true if this option requires an argument
//		uint8_t numeric:1;	// true if this option is numeric
//	} type;
//	char short_opt;			// short option
//	char * env_name;		// environment variable name
//	void * default_val;		// default value - if numeric, int, otherwise, char *
//	void * value;			// variable value - if numeric, int *, otherwise, char **
//};

int main(int argc, const char * argv[]) {
	struct {
		int verbose, _1_, keepalive, _2_, timeout, _3_;
		char * hostname, * port, * source_type, * source_name;
	} cfg;
	
	struct config_var vars[] = {
//		{"name",			{0, f, r, n},	'-',	"ENV_NAME",				DEFAULT_VALUE,				&my_variable},
		{"verbose",			{0, 1, 0, 0},	'v',	NULL,					(void *)1,					&cfg.verbose},
		{"quiet",			{0, 1, 0, 0},	'q',	NULL,					(void *)-1,					&cfg.verbose},
		{"hostname",		{0, 0, 1, 0},	'h',	"HOSTNAME",				"localhost",				&cfg.hostname},
		{"port",			{0, 0, 1, 0},	'p',	"PORT",					SERVER_PORT,				&cfg.port},
		{"source-type",		{0, 0, 1, 0},	't',	"SOURCE_TYPE",			"file",						&cfg.source_type},
		{"source-name",		{0, 0, 1, 0},	's',	"SOURCE_NAME",			NULL,						&cfg.source_name},
		{"keepalive-int",	{0, 0, 1, 1},	'k',	"KEEPALIVE_INTERVAL",	(void *)300,				&cfg.keepalive},
		{"packet-timeout",	{0, 0, 1, 1},	't',	"PACKET_TIMEOUT",		(void *)DEFAULT_TIMEOUT,	&cfg.timeout}
	};
	
	int sockfd = 0;
	ssize_t retv = 0;
	char buf[256];
	
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	pk_advertize_t * ad;
	
	pid_t keepalive_pid = 0;
	struct keepalive_param kp = {&sockfd, &cfg.keepalive};
	
	
	config(argc, argv, sizeof(vars)/sizeof(struct config_var), vars);
	
	ad = (pk_advertize_t *)alloc_packet(PACKET_SIZE_MAX);
	if ((retv = !ad))
		goto exit;
	
	printf("Connecting to %s:%s\n", cfg.hostname, cfg.port);
	if ((retv = connect_socket(cfg.hostname, cfg.port, &sockfd))) {
		fprintf(stderr, "Connection failed\n");
		goto free;
	}
	
	if ((retv = send_handshake(sockfd, cfg.timeout))) {
		fprintf(stderr, "Bad handshake\n");
		goto free;
	}
	
	
	_fork(&keepalive_pid, &do_keepalive, &kp);
	
	
	if (!strcasecmp("file", cfg.source_type)) {
		FILE * file = fopen(cfg.source_name, "r");
		if ((retv = !file)) {
			perror("fopen");
			goto free;
		}
		
		// states:
		//  0 - whitespace before name
		//  1 - name
		//  2 - whitespace between name and port
		//  3 - port
		//  4 - whitespace after port
		char state = 0, name[220], port[7];
		for (int c = 0, i = 0, j = 0; (c = fgetc(file)) > 0;)
			switch (state) {
				case 0: // whitespace before name
					if (c == '#') {
						state = 4;
						break;
					} else if (c <= 0x20 || 0x7F <= c)
						break;
					state = 1;
					i = 0;
					
				case 1: // name
					if ((retv = i >= sizeof(name))) {
						fprintf(stderr, "Names cannot be more than %lu characters\n", sizeof(name) - 1);
						goto fclose;
					} else if (j == 0 && 0x30 <= c && c <= 0x39) {
						// if the first character is a number, interpret it as a port
						port[j++] = c;
						state = 3;
						break;
					} if (0x20 < c && c < 0x7F) {
						name[i++] = c;
						break;
					}
					state = 2;
					
				case 2: // whitespace between name and port
					if (c <= 0x20 || 0x7F <= c)
						break;
					state = 3;
					j = 0;
					
				case 3: // port
					if ((retv = j >= sizeof(port))) {
						fprintf(stderr, "Port number cannot be greater than 65535\n");
						goto fclose;
					} else if (0x30 <= c && c <= 0x39) {
						port[j++] = c;
						state = 3;
						break;
					} else if (0x20 < c && c < 0x7F && c != '#') {
						fprintf(stderr, "Port numbers must be numeric\n");
						goto fclose;
					}
					state = 4;
					
				case 4: // whitespace after port
					if (c == '\n' || c == '\r') {
						name[i] = 0;
						port[j] = 0;
						
						if (i == 0 && j == 0) {
							state = 0;
							break;
						}
						
						int portnum = atoi(port);
						if ((retv = portnum > USHRT_MAX)) {
							fprintf(stderr, "Port number cannot be greater than 65535\n");
							goto fclose;
						}
						
						init_pk_advertize(ad, portnum, name);
						
						if (!*name)
							ad->name.data[0] = '+';
						
						retv = pk_send(sockfd, (pk_keepalive_t *)ad, 0); if ((retv = retv < 0)) goto free;
						
						state = 0;
					}
					break;
					
				default:
					fprintf(stderr, "Internal error\n");
					retv = 1;
					goto fclose;
					break;
			}
		
	fclose:
		fclose(file);
	} else if (!strcasecmp("sqlite", cfg.source_type)) {
		fprintf(stderr, "Using SQLite as a source is currently unsupported\n");
		retv = 1;
		goto free;
	} else {
		fprintf(stderr, "Bad source type %s\n", cfg.source_type);
		retv = 1;
		goto free;
	}
	
free:
	free_packet((pk_keepalive_t *)ad);
	
	while (!retv) {
		retv = pk_recv(sockfd, buf, cfg.timeout, 0);
		
		if (retv <= 0)
			break;
		
		retv = check_version(pk);
	}
	
//close:
	if (sockfd)
		close(sockfd);
	kill(keepalive_pid, SIGTERM);
exit:
	return (int)retv;
}

int do_keepalive(void * param) {
	struct keepalive_param * kp = param;
	pid_t ppid = getppid();
	pk_keepalive_t * pk = make_pk_keepalive(PK_KEEPALIVE);
	
	while (!kill(ppid, 0)) {
		pk_send(*kp->sockfd, pk, 0);
		_sleep(*kp->interval * 5000);
	}
	
	pk->type = PK_EXITING;
	pk_send(*kp->sockfd, pk, 0);
	
	close(*kp->sockfd);
	return 0;
}