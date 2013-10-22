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
		pk_send(sockfd, pk, 0);
		_sleep(5000);
	}
	
	close(sockfd);
	return 0;
}

int main(int argc, const char * argv[]) {
	pid_t keepalive_pid;
	int sockfd;
	ssize_t retv = 0;
	char buf[256];
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	pk_advertize_t * ad;
	
	ad = (pk_advertize_t *)alloc_packet(PACKET_SIZE_MAX);
	if ((retv = !ad))
		goto exit;
	
	if ((retv = connect_socket(NULL, SERVER_PORT, &sockfd)))
		goto exit;
	
	if ((retv = send_handshake(sockfd))) {
		fprintf(stderr, "Bad handshake\n");
		goto close;
	}
	
	_fork(&keepalive_pid, &do_keepalive, &sockfd);
	
	// send all of the services
	{
		init_pk_advertize(ad, 7777, "Terraria");
		retv = pk_send(sockfd, (pk_keepalive_t *)ad, 0); if (retv) goto free;
		
		init_pk_advertize(ad, 80, "HTTP");
		retv = pk_send(sockfd, (pk_keepalive_t *)ad, 0); if (retv) goto free;
		
		init_pk_advertize(ad, 22, "SSH");
		retv = pk_send(sockfd, (pk_keepalive_t *)ad, 0); if (retv) goto free;
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