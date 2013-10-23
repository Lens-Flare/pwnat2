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

#include "../common/network.h"
#include "../common/common.h"

int do_keepalive(void * param);

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
		int verbose;
		char * source_type;
		char * source_name;
		int keepalive;
	} cfg;
	
	pid_t keepalive_pid;
	int sockfd;
	ssize_t retv = 0;
	char buf[256];
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	pk_advertize_t * ad;
	
	struct config_var vars[] = {
		{"verbose",			{0, 1, 0, 0}, 'v', NULL,					(void *)1,		&cfg.verbose},
		{"quiet",			{0, 1, 0, 0}, 'q', NULL,					(void *)-1,		&cfg.verbose},
		{"source-type",		{0, 0, 1, 0}, 't', "SOURCE_TYPE",			"file",			&cfg.source_type},
		{"source-name",		{0, 0, 1, 0}, 's', "SOURCE_NAME",			NULL,			&cfg.source_name},
		{"keepalive-int",	{0, 0, 1, 1}, 'k', "KEEPALIVE_INTERVAL",	(void *)5000,	&cfg.keepalive}
	};
	
	config(argc, argv, sizeof(vars)/sizeof(struct config_var), vars);
	
	printf("verbose: %d\n", cfg.verbose);
	printf("source_type: %s\n", cfg.source_type);
	printf("source_name: %s\n", cfg.source_name);
	printf("keepalive: %d\n", cfg.keepalive);
	
	ad = (pk_advertize_t *)alloc_packet(PACKET_SIZE_MAX);
	if ((retv = !ad))
		goto exit;
	
	if ((retv = connect_socket("localhost", SERVER_PORT, &sockfd)))
		goto exit;
	
	if ((retv = send_handshake(sockfd))) {
		fprintf(stderr, "Bad handshake\n");
		goto close;
	}
	
	_fork(&keepalive_pid, &do_keepalive, &sockfd);
	
	// send all of the services
	{
		init_pk_advertize(ad, 7777, "Terraria");
		retv = pk_send(sockfd, (pk_keepalive_t *)ad, 0); if (retv < 0) goto free;
		
		init_pk_advertize(ad, 80, "");
		ad->name.data[0] = '+';
		retv = pk_send(sockfd, (pk_keepalive_t *)ad, 0); if (retv < 0) goto free;
		
		init_pk_advertize(ad, 22, "SSH");
		retv = pk_send(sockfd, (pk_keepalive_t *)ad, 0); if (retv < 0) goto free;
	}
	
free:
	free_packet((pk_keepalive_t *)ad);
	
	while (retv) {
		retv = pk_recv(sockfd, buf, 0);
		
		if (retv <= 0)
			break;
		
		retv = check_version(pk);
	}
	
close:
	close(sockfd);
exit:
	return (int)retv;
}

int do_keepalive(void * param) {
	int sockfd = *(int *)param;
	pk_keepalive_t * pk = make_pk_keepalive(PK_KEEPALIVE);
	
	while (1) {
		pk_send(sockfd, pk, 0);
		_sleep(5000);
	}
	
	close(sockfd);
	return 0;
}