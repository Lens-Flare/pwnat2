//
//  listening.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include "network.h"

int listener(const char * port, int backlog, pid_t * cpid) {
	int sockfd;
	
	if (!open_socket(NULL, port, backlog, &sockfd)) {
		return 1;
	}
	
	if (listen(sockfd, backlog) == -1) {
		perror("listen");
		return 1;
	}
	
	if (!(*cpid = fork()))
		while (1) {
			
		}
	
	return 0;
}