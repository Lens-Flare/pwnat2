//
//  main.c
//  server
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>

#include "server.h"
#include "../network/network.h"

int main(int argc, const char * argv[])
{
    struct sigaction sa;
    sa.sa_handler = _waitpid_sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
	
	fork_listener(SERVER_PORT, 10, NULL);
	
	return 0;
}

