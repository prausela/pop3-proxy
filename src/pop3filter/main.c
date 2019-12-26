/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "services/pop3/include/pop3_suscriptor.h"
#include "utils/include/lib.h"
#include "services/pop3/nio/include/pop3_client_nio.h"

#define FILTER_DEFAULT_IPV4_ADDRESS	INADDR_ANY
#define FILTER_DEFAULT_IPV6_ADDRESS	in6addr_any
#define FILTER_DEFAULT_PORT 		1110
#define CTL_DEFAULT_ADDRESS			((in_addr_t) 16777343) //inet_addr("127.0.0.1")
#define CTL_DEFAULT_PORT			9090

static void
sigterm_handler(const int signal) {
	printf("signal %d, cleaning up and exiting\n",signal);
	done = true;
	exit(0);
}

/*
static
int
param_validation(const int argc, const char **argv);

static
int
init_socket(const unsigned port, const char **err_msg);

static
selector_status
init_suscription_service(const int server, const char **err_msg);
*/

/*static
int
param_validation(int argc,char **argv,char* proxy_address, char* proxy_address_ipv6, char* client_port, char* admin_address, char* admin_address_ipv6, char* admin_port, char* origin_address, char* origin_port){
	command_line_parser(argc, argv, proxy_address, proxy_address_ipv6, client_port, admin_address, admin_address_ipv6, admin_port, origin_address, origin_port);
	return 0;
}*/

typedef union {
	struct sockaddr_in 	any;
	struct sockaddr_in6 ipv6;
} sockaddr_in_any;

static
int
init_socket(struct addrinfo *addrinfo, unsigned port, const char **err_msg){
	const int server = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	
	sockaddr_in_any addr;

	switch (addrinfo->ai_family)
	{
		case AF_INET6:
			memset(&addr.ipv6, 0, sizeof(addr.ipv6));
			addr.ipv6.sin6_family      	= addrinfo->ai_family;
			addr.ipv6.sin6_addr 		= ((struct sockaddr_in6*)addrinfo->ai_addr)->sin6_addr;
			addr.ipv6.sin6_port        	= htons(port);
			break;
		
		default:
			memset(&addr.any, 0, sizeof(addr.any));
			addr.any.sin_family      	= addrinfo->ai_family;
			addr.any.sin_addr.s_addr	= *((uint32_t*) & (((struct sockaddr_in*)addrinfo->ai_addr)->sin_addr));
			addr.any.sin_port        	= htons(port);
			break;
	}
    
    if(server < 0) {
        err_msg = "unable to create socket";
        return -1;
    }

    //fprintf(stdout, "Listening on TCP port %d\n", port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        return -1;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        return -1;
    }
	return server;
}

static
int
init_socket_with_addrinfo(const char* host, int family, unsigned port, struct addrinfo *hints, struct addrinfo **addrinfo, char **err_msg){
	
	hints->ai_family 	= family;
	
	int errcode = getaddrinfo(host, NULL, hints, addrinfo);
	
	if(errcode != 0){
		perror("getaddrinfo");
		return -1;
	} else {
		const int server = init_socket(*addrinfo, port, err_msg);
		freeaddrinfo(*addrinfo);
		return server;
	}
	return -1;
}

int
print_err_msg(selector_status ss, const char **err_msg){
	int ret = 0;
	if(ss != SELECTOR_SUCCESS) {
		fprintf(stderr, "%s: %s\n", (*err_msg == NULL) ? "": *err_msg,
								  ss == SELECTOR_IO
									  ? strerror(errno)
									  : selector_error(ss));
		ret = 2;
	} else if(*err_msg) {
		perror(*err_msg);
		ret = 1;
	}
	return ret;
}

int
main(const int argc, const char **argv) {
	

	int 		ret  	 = 0;
	struct pop3filter_arguments args = pop3filter_arguments_default;
	options_handler(argc, argv, &args);

	// No input from user is needed
	close(STDIN_FILENO);

	// Variable for error messages
	const char	*err_msg 	= NULL;
	
	struct addrinfo hints, *addrinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype 	= SOCK_STREAM;
	hints.ai_flags 		|= AI_CANONNAME;

	hints.ai_protocol	= IPPROTO_TCP;

	const int server_ipv4 = init_socket_with_addrinfo(args.client_address, AF_INET, args.client_port, &hints, &addrinfo, &err_msg);
	const int server_ipv6 = init_socket_with_addrinfo(args.client_address, AF_INET6, args.client_port, &hints, &addrinfo, &err_msg);
	
	hints.ai_protocol	= IPPROTO_SCTP;

	const int admin_ipv4 = init_socket_with_addrinfo(args.admin_address, AF_INET, args.admin_port, &hints, &addrinfo, &err_msg);
	const int admin_ipv6 = init_socket_with_addrinfo(args.admin_address, AF_INET6, args.admin_port, &hints, &addrinfo, &err_msg);
	
	// registrar sigterm es útil para terminar el programa normalmente.
	// esto ayuda mucho en herramientas como valgrind.
	signal(SIGTERM, sigterm_handler);
	signal(SIGINT,  sigterm_handler);

	while(1){

	}

	/*if(selector_fd_set_nio(server) == -1) {
		err_msg = "getting server socket flags";
		goto finally;
	}

	selector_status ss = init_suscription_service(server, admin, &err_msg);

finally:

	ret = print_err_msg(ss, &err_msg);

	if(server >= 0) {
		close(server);
	}
	*/
	return ret;
}
