//
//  listening.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <unistd.h>
#include <stdio.h>

#include "../network/network.h"

int do_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd) {
	int retv = 0;
	char buf[256];
	pk_keepalive_t * pk = (pk_keepalive_t *)buf;
	
	close(sockfd);
	
	if ((retv = recv_handshake(acptfd)))
		goto close;
	
	while (1) {
		pk_keepalive_t * bad;
		
		if ((retv = pk_recv(acptfd, buf, 0) < 0))
			goto close;
		
		if ((retv = check_version(pk))) {
			if (retv != NET_ERR_SERVER_BAD_SOFTWARE_VERSION && retv != NET_ERR_SERVER_BAD_NETWORK_VERSION) {
				pk_type_t type;
				
				switch (retv) {
					case NET_ERR_BAD_MAJOR_VERSION:
					case NET_ERR_BAD_MINOR_VERSION:
					case NET_ERR_BAD_REVISION:
					case NET_ERR_BAD_SUBREVISION:
						type = PK_BADSWVER;
						break;
					case NET_ERR_BAD_NETWORK_VERSION:
						type = PK_BADNETVER;
						break;
					default:
						goto close;
						break;
				}
				
				if ((retv = !(bad = make_pk_keepalive(type))))
					goto close;
				
				pk_send(acptfd, bad, 0);
				free_packet(bad);
			}
			goto close;
		}
		
		switch (pk->type) {
			case PK_KEEPALIVE:
				break;
			
			case PK_ADVERTIZE:
				// update list
				break;
				
			case PK_REQUEST:
				// send list
				break;
				
			case PK_FORWARD:
				// forward packet
				break;
				
			default:
				if ((retv = !(bad = make_pk_keepalive(PK_BADPACKET))))
					goto close;
				
				if ((retv = pk_send(acptfd, bad, 0) < 0)) {
					free_packet(bad);
					goto close;
				}
				break;
		}
	}
	
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
