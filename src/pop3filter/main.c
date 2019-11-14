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
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "services/pop3/include/pop3_suscriptor.h"
#include "../utils/lib.h"
#include "./services/pop3/nio/include/pop3_client_nio.h"

static void
sigterm_handler(const int signal) {
	printf("signal %d, cleaning up and exiting\n",signal);
	done = true;
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

static
int
param_validation(const int argc, const char **argv, char* client_port, char* admin_port, char *proxy_address, char* origin_port, char* origin_address){
	command_line_parser(argc, argv, client_port, admin_port, proxy_address, origin_port, origin_address);
	return 0;
}

static 
int 
init_socket(const char * proxy_port, const char * proxy_address, const char **err_msg, int protocol, int is_origin, char * origin_address, char * origin_port){
	struct sockaddr_in addr;
	struct sockaddr_in6 addr_ipv6;
	struct sockaddr_in addr_ipv4_aux;
	memset(&addr, 0, sizeof(addr));
	memset(&addr_ipv4_aux, 0, sizeof(addr_ipv4_aux));
	memset(&addr_ipv6, 0, sizeof(addr_ipv6));
	int server;
	int ans;
	fprintf(stderr,"HOLASSSSS %s\n", proxy_address);
	if(inet_pton(AF_INET, proxy_address, &(addr.sin_addr)))
	{
		fprintf(stderr,"HOLAAA1\n");
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = inet_addr(proxy_address);
		addr.sin_port        = htons(atoi(proxy_port));
		server = socket(AF_INET, SOCK_STREAM, protocol);
		if(server < 0) {
			*err_msg = "unable to create socket";
			return server;
		}
		if(is_origin){
			addr_ipv4_aux.sin_family      = AF_INET;
			addr_ipv4_aux.sin_addr.s_addr = inet_addr(origin_address);
			addr_ipv4_aux.sin_port        = htons(atoi(origin_port));
			fprintf(stderr,"Valor 1: %d\n",addr_ipv4_aux.sin_port);
			fprintf(stderr,"Valor 2: %d\n",addr_ipv4_aux.sin_addr.s_addr);
			init_curr_origin_server(req_addrtype_ipv4,NULL, addr_ipv4_aux.sin_port,addr_ipv4_aux,addr_ipv6);
		}
	}else if(inet_pton(AF_INET6, proxy_address, &(addr_ipv6.sin6_addr))){
		fprintf(stderr,"HOLAAA2\n");
		addr_ipv6.sin6_family      = AF_INET6;
		addr_ipv6.sin6_addr		   = in6addr_any;
		addr_ipv6.sin6_port        = htons(atoi(proxy_port));
		server = socket(AF_INET6, SOCK_STREAM, protocol);
		if(server < 0) {
			*err_msg = "unable to create socket";
			return server;
		}
		if(is_origin)
			init_curr_origin_server(req_addrtype_ipv6,NULL,addr_ipv6.sin6_port,addr,addr_ipv6);
	}else{
		//Resolucion del nombre
		
		fprintf(stderr,"HOLAAA3\n");
		addr.sin_family      = AF_UNSPEC;
		addr.sin_addr.s_addr = inet_addr(proxy_address);
		addr.sin_port        = htons(atoi(proxy_port));
		server = socket(AF_UNSPEC, SOCK_STREAM, protocol);
		if(server < 0) {
			*err_msg = "unable to create socket";
			return server;
		}
		if(is_origin)
			init_curr_origin_server(req_addrtype_domain,proxy_address,addr.sin_port,addr,addr_ipv6);
	}
	
	if(protocol == IPPROTO_TCP){
		fprintf(stdout, "Listening on TCP port %s\n", proxy_port);
	}
	else if(protocol == IPPROTO_SCTP){
		fprintf(stdout, "Listening on SCTP port %s\n", proxy_port);
	}
	// man 7 ip. no importa reportar nada si falla.
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	fprintf(stderr, "LLEGUE");
	if((ans = bind(server, (struct sockaddr*) &addr, sizeof(addr))) < 0) {
		*err_msg = "unable to bind socket";
		return ans;
	}

	if ((ans = listen(server, 20)) < 0) {
		*err_msg = "unable to listen";
		return ans;
	}

	return server;
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
	unsigned 	port 	 = 1100;
	char 		client_port[100];
	char 		admin_port[100];
	char 		proxy_address[100];
	char 		origin_port[100];
	char 		admin_address[100];
	char 		origin_address[100];

	strcpy(client_port, "1110");
	strcpy(origin_port, "110");
	strcpy(proxy_address, "0.0.0.0");
	strcpy(admin_port, "9090");
	strcpy(admin_address, "0.0.0.0");
	strcpy(origin_address, "127.0.0.1");
	//201.212.7.96

	if((ret = param_validation(argc, argv, client_port, admin_port, proxy_address, origin_port, origin_address)) != 0){
		return ret;
	}

	if(strcmp(origin_address, "localhost")==0){
		if((strcmp(origin_port, client_port))==0){
			printf("-ERR. Proxy server cannot match origin server\n");
			return 1;
		}
	}

	// No input from user is needed
	close(STDIN_FILENO);

	// Variable for error messages
	const char	*err_msg = NULL;
	const int 	server 	 = init_socket(client_port, proxy_address, &err_msg, IPPROTO_TCP,1,origin_address,origin_port);

	if(server < 0){
		goto finally;
	}

	const int 	admin 	 = init_socket(admin_port, admin_address, &err_msg, IPPROTO_SCTP,0,origin_address,origin_port);

	if(admin < 0){
		goto finally;
	}


	// registrar sigterm es útil para terminar el programa normalmente.
	// esto ayuda mucho en herramientas como valgrind.
	signal(SIGTERM, sigterm_handler);
	signal(SIGINT,  sigterm_handler);

	if(selector_fd_set_nio(server) == -1) {
		err_msg = "getting server socket flags";
		goto finally;
	}
	
	selector_status ss = init_suscription_service(server, admin, &err_msg);

finally:

	ret = print_err_msg(ss, &err_msg);

	if(server >= 0) {
		close(server);
	}
	return ret;
}