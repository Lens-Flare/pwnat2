//
//  rawnet.c
//  pwnat2
//
//  Created by Ethan Reesor on 10/23/13.
//  Copyright (c) 2013 Ethan. All rights reserved.
//

#include <stdint.h>
#include <netinet/in.h>

struct ip_header {
	uint8_t ver:4, ihl:4, dscp:6, ecn:2;
	uint16_t len;
	
	uint16_t id;
	uint16_t flags:3, offset:13;
	
	uint8_t ttl, proto;
	uint16_t checksum;
	
	in_addr_t src;
	
	in_addr_t dst;
	
	uint32_t data[];
};