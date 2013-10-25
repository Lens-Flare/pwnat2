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
#include <string.h>

#include "../common/common.h"

struct {
	cfgint verbose, keepalive, timeout;
	char * var_setup, * hostname, * port, * source_type, * source_name;
} cfg;

typedef struct service_list {
	struct service_list * next;
	union {
		char buf[PACKET_SIZE_MAX];
		pk_service_t pk;
	} serv;
} service_list_t;

int ask_server_for_services(service_list_t ** head);

void usage(const char * argv0) {
	printf("Usage: %s [-0 prefix | list] [-q | -v] [-h hostname] [-p port] [-t timeout]\n", argv0);
}

int main(int argc, const char * argv[])
{
	struct config_var vars[] = {
		{"var-setup",		{ct_var, ct_req, ct_str},	'0',	NULL,							NULL,						&cfg.var_setup},
		{"verbose",			{ct_flg, 0     , 0     },	'v',	NULL,							(void *)1,					&cfg.verbose},
		{"quiet",			{ct_flg, 0     , 0     },	'q',	NULL,							(void *)-1,					&cfg.verbose},
		{"hostname",		{ct_var, ct_req, ct_str},	'h',	ENV_PREFIX"HOSTNAME",			"localhost",				&cfg.hostname},
		{"port",			{ct_var, ct_req, ct_str},	'p',	ENV_PREFIX"PORT",				SERVER_PORT,				&cfg.port},
		{"packet-timeout",	{ct_var, ct_req, ct_num},	't',	ENV_PREFIX"PACKET_TIMEOUT",		(void *)DEFAULT_TIMEOUT,	&cfg.timeout}
	};
	
	service_list_t * srvs;
	int ret;
	char s[INET6_ADDRSTRLEN];
	
	ret = config(argc, argv, ARRLEN(vars), (struct config_var *)vars);
	if (ret) {
		usage(argv[0]);
		return ret;
	}
	
	if (cfg.var_setup) {
		if (!strcasecmp("prefix", cfg.var_setup)) {
			printf(ENV_PREFIX"\n");
			return 0;
		} else if (!strcasecmp("list", cfg.var_setup)) {
			for (int i = 0; i < ARRLEN(vars); i++)
				if (vars[i].env_name)
					printf("%s\n", vars[i].env_name + ENV_PREFIX_LEN - 1);
			return 0;
		}
	}
	
	ret = ask_server_for_services(&srvs);
	if(ret)
		return ret;
	
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
	
	
	ret = connect_socket(cfg.hostname, cfg.port, &sockfd);
	if(ret)
	{
//		perror("consumer: open_socket");
		goto exit;
	}
	
	ret = send_handshake(sockfd, (int)cfg.timeout);
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
		
		ret = (int)pk_recv(sockfd, (char *)&current->serv.buf, (int)cfg.timeout, 0);
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
