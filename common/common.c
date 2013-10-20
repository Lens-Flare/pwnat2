/*
 * common.c
 *
 *  Created on: Oct 19, 2013
 *      Author: cody
 */

#ifdef WINDOWS
	#include <windows.h>
#else
	#include <unistd.h>
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