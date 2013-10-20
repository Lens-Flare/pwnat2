//
//  main.c
//  server
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>

#include "server.h"
#include "../network/network.h"

int main(int argc, const char * argv[])
{
	fork_listener(SERVER_PORT, 10, NULL);
	
	return 0;
}

