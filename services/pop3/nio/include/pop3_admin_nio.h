#ifndef POP3_ADMIN_NIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm
#define POP3_ADMIN_NIO_H_whVj9DjZzFKtzEUtC0Ma2Ae45Hm

#include <netdb.h>
#include "../../../include/selector.h"

/** handler del socket pasivo que atiende conexiones socksv5 */
void
pop3_admin_passive_accept(struct selector_key *key);


/** libera pools internos */
void
destroy_suscription_pool(void);

/*enum addr_type {
    req_addrtype_ipv4   = 0x01,
    req_addrtype_domain = 0x03,
    req_addrtype_ipv6   = 0x04,
};

union addr {
    char fqdn[0xff];
    struct sockaddr_in  ipv4;
    struct sockaddr_in6 ipv6;
};

struct server_credentials {
    enum  addr_type 		dest_addr_type;
    union addr				dest_addr;*/
    /** port in network byte order */
   /* in_port_t 				dest_port;
};

struct server_credentials curr_origin_server;
*/
#endif
