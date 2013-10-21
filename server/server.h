//
//  server.h
//  pwnat2
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#ifndef pwnat2_server_h
#define pwnat2_server_h

#include <unistd.h>

int fork_listener(const char * port, int backlog, pid_t * cpid);

#endif
