//
//  listening.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "server.h"
#include "../network/network.h"

int do_handler(int sockfd, struct sockaddr_storage addr, socklen_t addrlen, int acptfd) {
	int retv = 0;
	char buf[PACKET_SIZE_MAX];
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
				; pk_advertize_t * ad = (pk_advertize_t *)pk;
				printf("Provider advertizing %s on %d\n", (char *)&ad->name.data, ad->port);
				// update list
				break;
				
			case PK_REQUEST:
				printf("Client requesting services\n");
				// send list
				break;
				
			case PK_FORWARD:
				printf("Client requesting packet forwarding\n");
				// forward packet
				break;
				
			case PK_EXITING:
				printf("Client/provider exiting\n");
				goto close;
				
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
		exit(do_handler(sockfd, addr, addrlen, acptfd));
	return 0;
}

int do_listener(int sockfd) {
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int acptfd;
    struct sigaction sa;
	
    sa.sa_handler = _waitpid_sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
	
	while (1) {
		if ((acptfd = accept(sockfd, (struct sockaddr *)&addr, &addrlen)) < 0) {
			perror("accept");
			continue;
		}
		
		fork_handler(sockfd, addr, addrlen, acptfd);
		close(acptfd);
	}
	
	return 0;
}

int fork_listener(const char * port, int backlog, pid_t * cpid) {
	int sockfd;
	
	if (listen_socket(NULL, (char *)port, backlog, &sockfd)) {
		return 1;
	}
	
	if (cpid == NULL || !(*cpid = fork()))
		exit(do_listener(sockfd));
	
	return 0;
}
