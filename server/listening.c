//
//  listening.c
//  pwnat2
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

int listener(const char * port, int backlog, pid_t * cpid) {
	int sockfd, retv, yes;
	struct addrinfo hints, *servinfo, *p;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if (!(retv = getaddrinfo(NULL, port, &hints, &servinfo))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retv));
        return 1;
	}
	
	for (p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            perror("server: socket");
            continue;
        }
		
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
            perror("setsockopt");
            return 1;
        }
		
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
		
        break;
	}
	
    if (!p)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }
	
	freeaddrinfo(servinfo);
	
    if (listen(sockfd, backlog) == -1) {
        perror("listen");
        exit(1);
    }
	
	if (!(*cpid = fork()))
		while (1) {
			
		}
	
	return 0;
}