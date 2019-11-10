#include <stdlib.h>
#include <netinet/in.h>
#include <stdio.h>

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

struct server_credentials {
    enum  addr_type 		dest_addr_type;
    union addr				dest_addr;
    /** port in network byte order */
    in_port_t 				dest_port;
};

struct server_credentials origin_server = {
	.dest_addr_type = req_addrtype_domain, 
	.dest_addr = {.fqdn = "pop.fibertel.com.ar"}, 
	.dest_port = 110,
};

int main(void){
	for(int i = 0; i < 1025; i++){
		system("nc -C localhost 1080 &");
		printf("Wuahahaha\n");
	}
}