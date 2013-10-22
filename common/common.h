//
//  common.h
//  pwnat2
//
//  Created by Ethan Reesor on 10/19/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#ifndef pwnat2_common_h
#define pwnat2_common_h

#define MAJOR_VERSION	0
#define MINOR_VERSION	1
#define REVISION		0
#define SUBREVISION		2

#define SERVER_PORT		"45678"

#define _perror()		perror(__FUNCTION__);


typedef enum {TRUE = 1, FALSE = 0} boolean;
typedef int errcode;

void _sleep(int millis);
int _fork(pid_t * cpid, int (*run)(void *), void * params);
int _random(void * data, size_t len);
void _waitpid_sigchld_handler(int s);

#endif
