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
	int sockfd, retv = 0;
	char buf[256];
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	
	if ((retv = !open_socket(NULL, SERVER_PORT, &sockfd)))
		goto exit;
	
	if ((retv = send_handshake(sockfd)))
		goto close;
	
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
		
		if ((retv = check_version(pk)))
			goto close;
	}
	
close:
	close(sockfd);
exit:
	return retv;
}