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
#include "consumer.h"
#include "../network/network.h"
#include "../common/common.h"

int main(int argc, const char * argv[])
{
	pk_service_t * srvs;
	int numSrvs;
	char s[INET6_ADDRSTRLEN];

	numSrvs = ask_server_for_services(&srvs);

	for(int i=0; i<numSrvs; i++)
	{
		inet_ntop(srvs[i].address.family, &srvs[i].address.data, s, sizeof s);
		printf("%s @ %s:%d", srvs[i].name.data, s, srvs[i].port);
	}

	return 0;
}

int ask_server_for_services(pk_service_t ** srvs)
{
	int sockfd;
	int num_serv;
	int ret;
	int i = -1;
	int paclen = sizeof(pk_keepalive_t);
	pk_keepalive_t * packet;
	char buf[PACKET_SIZE_MAX];

	ret = connect_socket("computingeureka.com", SERVER_PORT, &sockfd);
	if(ret)
	{
		perror("consumer: open_socket");
		goto exit;
	}

//	Make a request packet.
	packet = make_pk_keepalive(PK_REQUEST);
	if(!packet)
	{
		perror("consumer: make_pk_keepalive");
		goto close_sock;
	}

//	Send the request.
	ret = pk_send(sockfd, packet, 0);
	if(ret != paclen)
	{
		perror("consumer: pk_send");
		goto free_pk;
	}

//	Receive the response
	ret = pk_recv(sockfd, buf, 0);
	if(ret < 0)
	{
		perror("consumer: pk_recv");
		goto free_pk;
	}
	ret = check_version((pk_keepalive_t*)buf);
	if(ret)
	{
		perror("consumer: check_version");
		goto free_pk;
	}

	num_serv = ((pk_response_t*)buf)->services;

//	malloc() the required space.
	*srvs = malloc(PACKET_SIZE_MAX * num_serv);
	if(!*srvs)
	{
		perror("consumer: malloc");
		goto free_pk;
	}

//	Loop for recv and copy
	for(i=0; i<num_serv; i++)
	{
		ret = pk_recv(sockfd, ((void*)*srvs+256*i), 0);
		if(ret < 0)
		{
			perror("consumer: pk_recv");
			goto free_pk;
		}
	}

free_pk:
	free(packet);
close_sock:
	close(sockfd);
exit:
	return i;
}
