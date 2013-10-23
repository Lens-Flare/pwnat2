//
//  main.c
//  consumer
//
//  Created by Ethan Reesor on 10/16/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "../common/network.h"
#include "../common/common.h"

typedef struct service_list {
	struct service_list * next;
	union {
		char buf[PACKET_SIZE_MAX];
		pk_service_t pk;
	} serv;
} service_list_t;

int ask_server_for_services(service_list_t ** head);

int main(int argc, const char * argv[])
{
	service_list_t * srvs;
	int ret;
	char s[INET6_ADDRSTRLEN];
	
	ret = ask_server_for_services(&srvs);
	
	for (service_list_t * current = srvs; current && current->serv.pk._super.type != PK_RESPONSE; current = current->next)
	{
		inet_ntop(current->serv.pk.address.family, &current->serv.pk.address.data, s, sizeof(s));
		printf("%s @ %s:%d\n", (char *)&current->serv.pk.name.data, s, current->serv.pk.port);
	}
	
	for (service_list_t * old, * current = srvs; current; free(old)) {
		old = current;
		current = current->next;
	}
	
	return 0;
}

int ask_server_for_services(service_list_t ** head)
{
	int sockfd;
	int ret = 0;
//	int paclen = sizeof(pk_keepalive_t);
	pk_keepalive_t * packet;
	
	
	ret = connect_socket("localhost", SERVER_PORT, &sockfd);
	if(ret)
	{
//		perror("consumer: open_socket");
		goto exit;
	}
	
	ret = send_handshake(sockfd, DEFAULT_TIMEOUT);
	if (ret) {
		fprintf(stderr, "Bad handshake\n");
		goto close_sock;
	}
	
//	Make a request packet.
	packet = make_pk_keepalive(PK_REQUEST);
	if(!packet)
	{
//		perror("consumer: make_pk_keepalive");
		goto close_sock;
	}
	
//	Send the request.
	ret = (int)pk_send(sockfd, packet, 0);
	if(ret < 0)
	{
//		perror("consumer: pk_send");
		goto free_pk;
	}
	
	service_list_t * current;
	*head = NULL;
	do {
		if (!*head)
			*head = current = calloc(1, sizeof(service_list_t));
		else
			current = current->next = calloc(1, sizeof(service_list_t));
		
		ret = (int)pk_recv(sockfd, (char *)&current->serv.buf, DEFAULT_TIMEOUT, 0);
		if(ret < 0)
		{
			//		perror("consumer: pk_recv");
			goto free_list;
		}
		ret = check_version((pk_keepalive_t *)&current->serv.pk);
		if(ret)
		{
			//		perror("consumer: check_version");
			goto free_list;
		}
	} while (current && current->serv.pk._super.type == PK_SERVICE);
	
	if (!current)
		perror("calloc");
	
	if (current->serv.pk._super.type != PK_RESPONSE)
		fprintf(stderr, "The service list did not end with a response packet\n");
	
free_pk:
	free(packet);
close_sock:
	close(sockfd);
exit:
	return ret;
free_list:
	current = *head;
	for (service_list_t * old; current; free(old)) {
		old = current;
		current = current->next;
	}
	goto free_pk;
}
