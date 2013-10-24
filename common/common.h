//
//  common.h
//  pwnat2
//
//  Created by Ethan Reesor on 10/19/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#ifndef pwnat2_common_h
#define pwnat2_common_h

#include <stdint.h>

#define STRLEN(s)		(sizeof(s)/sizeof(s[0])

#define MAJOR_VERSION	0
#define MINOR_VERSION	1
#define REVISION		1
#define SUBREVISION		0

#define SERVER_PORT		"45678"
#define DEFAULT_TIMEOUT	60
#define ENV_PREFIX		"_PWNAT_"
#define ENV_PREFIX_LEN	STRLEN(ENV_PREFIX)

#define _perror()		perror(__FUNCTION__);

struct config_var {
	char * name;			// long option
	struct {
		uint8_t res:5;
		uint8_t flag:1;		// true if this option is a flag - implies numeric and no argument
		uint8_t required:1;	// true if this option requires an argument
		uint8_t numeric:1;	// true if this option is numeric
	} type;
	char short_opt;			// short option
	char * env_name;		// environment variable name
	void * default_val;		// default value - if numeric, int, otherwise, char *
	void * value;			// variable value - if numeric, int *, otherwise, char **
	
	// CAVEAT: int values must be castable to char *; if the size of a pointer
	//   is larger than the size of the value storage, nearby values will be
	//   overwritten
};


typedef enum {TRUE = 1, FALSE = 0} boolean;
typedef int errcode;

void _sleep(int millis);
int _fork(pid_t * cpid, int (*run)(void *), void * params);
int _random(void * data, size_t len);
void _waitpid_sigchld_handler(int s);

int config(int argc, const char * argv[], int num_vars, struct config_var * vars);

#endif
