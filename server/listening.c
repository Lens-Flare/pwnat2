//
//  listening.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <unistd.h>
#include <stdio.h>

#include "network.h"

int do_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd) {
	int retv = 0;
	char buf[256];
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	
	close(sockfd);
	
	if ((retv = recv_handshake(acptfd)))
		goto close;
	
close:
	close(acptfd);
	return retv;
}

int fork_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd) {
	if (!fork())
		return do_handler(sockfd, addr, addrlen, acptfd);
	return 0;
}

int do_listener(int sockfd) {
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int acptfd;
	
	while (1) {
		if ((acptfd = accept(sockfd, (struct sockaddr *)&addr, &addrlen)) < 0) {
			perror("accept");
			continue;
		}
		
		fork_handler(sockfd, addr, addrlen, acptfd);
		close(acptfd);
	}
}

int fork_listener(const char * port, int backlog, pid_t * cpid) {
	int sockfd;
	
	if (!open_socket(NULL, (char *)port, &sockfd)) {
		return 1;
	}
	
	if (listen(sockfd, backlog) == -1) {
		perror("listen");
		return 1;
	}
	
	if (!(*cpid = fork()))
		do_listener(sockfd);
	
	return 0;
}