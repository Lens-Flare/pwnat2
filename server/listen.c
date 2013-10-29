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
#include <string.h>
#include <sys/msg.h>

#define LISTEN

#include "server.h"

struct {
	int sockfd, acptfd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
} stt_listen;

int server_listen() {
	int retv;
	
	memset(&stt_listen, 0, sizeof(stt_listen));
	
	stt_listen.addrlen = sizeof(stt_listen.addr);
	
	retv = listen_socket(cfg.listen, cfg.port, (int)cfg.backlog, &stt_listen.sockfd);
	if (retv) {
		retv = SEC_LISTEN;
        goto _return;
	}
	
	retv = server_s_sigaction();
	if (retv)
		goto _return;
	
	for (retv = 0; !retv; retv = server_l_accept());
	
	if (stt_main.pid == getppid()) {
		retv = msgctl(stt_main.queue_id, IPC_RMID, NULL);
		if (retv < 0) {
			perror("msgctl");
			retv = SEC_MSGCTL;
			goto _return;
		}
	}
	
_return:
	return retv;
}

int server_l_accept() {
	int retv = 0;
	pid_t cpid;
	
	stt_listen.acptfd = accept(stt_listen.sockfd, (struct sockaddr *)&stt_listen.addr, &stt_listen.addrlen);
	if (stt_listen.acptfd < 0) {
		perror("accept");
		retv = SEC_ACCEPT;
		goto _return;
	}
	
	if (!(cpid = fork()))
		exit(server_handle());
	
	close(stt_listen.acptfd);
	
_return:
	return retv;
}