/*
 * common.c
 *
 *  Created on: Oct 19, 2013
 *      Author: cody
 */

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
	#include <stdio.h>
	#include <sys/wait.h>
	#include <signal.h>
#endif


#include "common.h"

void _sleep(int millis)
{
	#ifdef WINDOWS
		Sleep(millis);
	#else
		usleep(1000*millis);
	#endif
}

int _fork(pid_t * cpid, int (*run)(void *), void * params) {
	if (!(*cpid = fork()))
		return (*run)(params);
	return 0;
}

int _random(void * data, size_t len) {
	int randint;
	
	#ifndef WINDOWS
		FILE * fp = fopen("/dev/urandom", "r");
		if (!fp)
			return 1;
	#endif
	
	for (int i = 0; i < len; i++) {
		#ifdef WINDOWS
			if (i % 4 == 0)
				randint = rand();
			((char *)data)[i] = randint >> (8 * (i % 4));
		#else
			if ((randint = fgetc(fp)) < 0)
				return 2;
			((char *)data)[i] = randint;
		#endif
	}
	
	#ifndef WINDOWS
		fclose(fp);
	#endif
	
	return 0;
}

void _waitpid_sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}