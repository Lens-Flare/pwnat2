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

#include "../network/network.h"
#include "../common/common.h"

int do_keepalive(void * param) {
	int sockfd = *(int *)param;
	pk_keepalive_t * pk = make_pk_keepalive(PK_KEEPALIVE);
	
	while (1) {
		send(sockfd, pk, pk->size, 0);
		_sleep(5000);
	}
	
	return 0;
}

int main(int argc, const char * argv[]) {
	pid_t keepalive_pid;
	int sockfd;
	char buf[256];
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	
	if (!open_socket(NULL, SERVER_PORT, &sockfd)) {
		return 1;
	}
	
	_fork(&keepalive_pid, &do_keepalive, &sockfd);
	
	// send all of the services
	{
		pk_advertize_t * ad = make_pk_advertize(7777, "Terraria");
		pk_send(sockfd, (pk_keepalive_t *)ad, 0);
		free_packet((pk_keepalive_t *)ad);
		
		ad = make_pk_advertize(80, "HTTP");
		pk_send(sockfd, (pk_keepalive_t *)ad, 0);
		free_packet((pk_keepalive_t *)ad);
		
		ad = make_pk_advertize(22, "SSH");
		pk_send(sockfd, (pk_keepalive_t *)ad, 0);
		free_packet((pk_keepalive_t *)ad);
	}
	
	while (1) {
		// pk == buf
		pk_recv(sockfd, buf, 0);
		
		switch (pk->type) {
			case PK_BADSWVER:
				fprintf(stderr, "Incompatible software version (%d.%d-r%d.%d)", pk->version[0], pk->version[1], pk->version[2], pk->version[3]);
				return 1;
				break;
			case PK_BADNETVER:
				fprintf(stderr, "Incompatible network structure version (%d)", pk->netver);
				return 1;
				break;
			default:
				break;
		}
	}
	
	return 0;
}