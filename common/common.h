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
#define SUBREVISION		0

#define SERVER_PORT		"45678"

void _sleep(int millis);
int _fork(pid_t * cpid, int (*run)(void *), void * params);

#endif
