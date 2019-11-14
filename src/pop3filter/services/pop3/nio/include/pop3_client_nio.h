#ifndef POP3_CLIENT_NIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm
#define POP3_CLIENT_NIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "../../../include/buffer.h"

#include "../../../include/stm.h"
#include "../../../../utils/include/netutils.h"
#include "../../parsers/include/pop3_command_parser.h"
#include "../../parsers/include/pop3_singleline_response_parser.h"
#include "../../parsers/include/pop3_multiline_response_parser.h"
#include "../../include/pop3.h"
#include "../../../../../services/services.h"
#include "../../../include/selector.h"
#include "../../../../utils/include/structure_builder.h"
#include "pop3_admin_nio.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

#define GREETING_OK "+OK Welcome to pop3filter!\r\n"



/** handler del socket pasivo que atiende conexiones socksv5 */
void
init_curr_origin_server(int address_type,char addr[0xff], in_port_t dest_port,struct sockaddr_in ipv4,struct sockaddr_in6 ipv6 );
void
pop3_passive_accept(struct selector_key *key);


/** libera pools internos */
void
destroy_suscription_pool(void);

enum addr_type {
    req_addrtype_ipv4   = 0x01,
    req_addrtype_domain = 0x03,
    req_addrtype_ipv6   = 0x04,
};

union addr {
    char fqdn[0xff];
    struct sockaddr_in  ipv4;
    struct sockaddr_in6 ipv6;
};
/*
.ipv4 = {
			.sin_port = 28160,
			.sin_family = AF_INET, 
			.sin_addr = {
	    		s_addr = 1611125961
	  		},
	  		sin_zero = "\200PUUUU\000"
	  	}},*/


struct server_credentials {
    enum  addr_type 		dest_addr_type;
    union addr				dest_addr;
    /** port in network byte order */
    in_port_t 				dest_port;
};
struct server_credentials * curr_origin_server;


#endif
