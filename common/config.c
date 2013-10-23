//
//  config.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/22/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <limits.h>

#include "common.h"

int config(int argc, const char * argv[], int num_vars, struct config_var * vars) {
	struct config_var * var, * lookup[256] = {
		0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
		0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
		0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
		0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0
	};
	
	char shortopts[3*num_vars + 1];
	struct option longopts[num_vars	+ 1];
	
	for (int i = 0, j = 0; i <= num_vars; i++) {
		// get the current configuration variable
		var = i < num_vars ? vars + i : NULL;
		
		// set the value to the default or the environment variable
		if (var) {
			if (var->env_name)
				*(char **)var->value = getenv(var->env_name);
			else
				*(char **)var->value = 0;
			
			if (*(char **)var->value && var->type.flag)
				*(int *)var->value = atoi(*(char **)var->value);
			else if (!*(char **)var->value && !var->type.flag && !var->type.numeric && var->default_val)
				*(char **)var->value = var->default_val;
		}
		
		// set up the longopt struct
		if (!var)
			longopts[i] = (struct option){0, 0, 0, 0};
		else if (var->type.flag)
			longopts[i] = (struct option){var->name, no_argument, (int *)var->value, (int)var->default_val};
		else if (var->type.required)
			longopts[i] = (struct option){var->name, required_argument, NULL, var->short_opt};
		else
			longopts[i] = (struct option){var->name, optional_argument, NULL, var->short_opt};
		
		// set up the shortopt
		if (!var)
			shortopts[j] = 0;
		else if (var->short_opt) {
			lookup[(int)var->short_opt] = var;
			shortopts[j++] = var->short_opt;
			
			if (!var->type.flag) {
				shortopts[j++] = ':';
				
				if (!var->type.required)
					shortopts[j++] = ':';
			}
		}
	}
	
	for (int c = '?', i = 0; c >= 0; c = getopt_long(argc, (char **)argv, shortopts, longopts, &i)) {
		switch (c) {
			case 0:
				if (!vars[i].type.flag)
					*(char **)vars[i].value = optarg;
				break;
				
			case '?':
				// do something?
				break;
				
			default:
				if ((var = lookup[c])) {
					if (var->type.flag)
						*(int *)var->value = (int)var->default_val;
					else if (optarg)
						*(char **)var->value = optarg;
				}
				
				break;
		}
	}
	
	for (int i = 0; i < num_vars && (var = vars + i); i++)
		if (var->type.numeric) {
			if (*(char  **)var->value)
				*(int *)var->value = atoi(*(char **)var->value);
			else
				*(int *)var->value = (int)var->default_val;
		}
		
	
	return 0;
}