//
//  listen.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/25/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#define LISTEN

#include "server.h"

struct {
	int sockfd, acptfd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
} stt_listen;

int server_listen() {
	int retv;
	
	retv = listen_socket(cfg.listen, cfg.port, (int)cfg.backlog, &stt_listen.sockfd);
	if (retv) {
		retv = SEC_LISTEN;
        goto _return;
	}
	
	retv = server_init_sig();
	if (retv)
		goto _return;
	
	for (retv = 0; !retv; retv = server_accept());
	
_return:
	return retv;
}

int server_accept() {
	int retv = 0;
	
	stt_listen.acptfd = accept(stt_listen.sockfd, (struct sockaddr *)&stt_listen.addr, &stt_listen.addrlen);
	if (stt_listen.acptfd < 0) {
		perror("accept");
		retv = SEC_ACCEPT;
		goto _return;
	}
	
	if (!fork())
		exit(server_handle());
	
	close(stt_listen.acptfd);
	
_return:
	return retv;
}