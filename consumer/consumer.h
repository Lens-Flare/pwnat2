/*
 * consumer.h
 *
 *  Created on: Oct 19, 2013
 *      Author: cody
 */

#ifndef CONSUMER_H_
#define CONSUMER_H_

#include "../network/network.h"

typedef struct service_list {
	struct service_list * next;
	union {
		char buf[PACKET_SIZE_MAX];
		pk_service_t pk;
	} serv;
} service_list_t;

int ask_server_for_services(service_list_t ** head);


#endif /* CONSUMER_H_ */
