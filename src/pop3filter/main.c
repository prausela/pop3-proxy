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
#include "utils/include/lib.h"
#include "services/pop3/nio/include/pop3_client_nio.h"

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
param_validation(int argc,char **argv,char* proxy_address, char* proxy_address_ipv6, char* client_port, char* admin_address, char* admin_address_ipv6, char* admin_port, char* origin_address, char* origin_port){
	command_line_parser(argc, argv, proxy_address, proxy_address_ipv6, client_port, admin_address, admin_address_ipv6, admin_port, origin_address, origin_port);
	return 0;
}

static
int
init_socket(const char * port, const char * address, const char **err_msg, int protocol, int is_origin){
	struct sockaddr_in addr;
	struct sockaddr_in6 addr_ipv6;
	struct sockaddr_in addr_ipv4_aux;
	memset(&addr, 0, sizeof(addr));
	memset(&addr_ipv4_aux, 0, sizeof(addr_ipv4_aux));
	memset(&addr_ipv6, 0, sizeof(addr_ipv6));
	int server;
	int ans;
	int ip=0;

	if(is_origin){
		printf("Creando origin server... la direccion es: %s\n",address );
		if(inet_pton(AF_INET, address, &(addr.sin_addr)))
		{
			printf("Es una direccion IPV4\n");
			addr_ipv4_aux.sin_family      = AF_INET;
			addr_ipv4_aux.sin_addr.s_addr = inet_addr(address);
			addr_ipv4_aux.sin_port        = htons(atoi(port));
			//fprintf(stderr,"Valor 1: %d\n",addr_ipv4_aux.sin_port);
			//fprintf(stderr,"Valor 2: %d\n",addr_ipv4_aux.sin_addr.s_addr);
			init_curr_origin_server(req_addrtype_ipv4,NULL, addr_ipv4_aux.sin_port,addr_ipv4_aux,addr_ipv6);
		}
		else if(inet_pton(AF_INET6, address, &(addr_ipv6.sin6_addr))){
			printf("Es una direccion IPV6\n");
			addr_ipv6.sin6_family      = AF_INET6;
			addr_ipv6.sin6_addr		   = in6addr_any;
			addr_ipv6.sin6_port        = htons(atoi(port));
			init_curr_origin_server(req_addrtype_ipv6,NULL,addr_ipv6.sin6_port,addr,addr_ipv6);
		}
		else{
			addr.sin_family      = AF_UNSPEC;
			addr.sin_addr.s_addr = inet_addr(address);
			addr.sin_port        = htons(atoi(port));
			init_curr_origin_server(req_addrtype_domain,address,addr.sin_port,addr,addr_ipv6);
		}
	}


	else{
		if(inet_pton(AF_INET, address, &(addr.sin_addr)))
		{
			ip=AF_INET;
			printf("Reconocio que la direccion pasada es IPV4: %s\n", address);
			addr.sin_family      = AF_INET;
			addr.sin_addr.s_addr = inet_addr(address);
			addr.sin_port        = htons(atoi(port));
			server = socket(AF_INET, SOCK_STREAM, protocol);
			if(server < 0) {
				*err_msg = "unable to create socket";
				return server;
			}

		}else if(inet_pton(AF_INET6, address, &(addr_ipv6.sin6_addr))){
			ip=AF_INET6;
			printf("Reconocio que la direccion pasada es IPV6: %s\n",address );
			server = socket(AF_INET6, SOCK_STREAM, 0);
			if(server < 0) {
				*err_msg = "unable to create socket";
				return server;
			}
			//printf("El valor de server es: %d\n",server );
			addr_ipv6.sin6_family      = AF_INET6;
			//addr_ipv6.sin6_addr		   = in6addr_any;
			addr_ipv6.sin6_port        = htons(atoi(port));
			}
			else {
				printf("Esto no deberia mostrarse (ejecucion de nombre)\n");
			//Resolucion del nombre
			addr.sin_family      = AF_UNSPEC;
			addr.sin_addr.s_addr = inet_addr(address);
			addr.sin_port        = htons(atoi(port));
			server = socket(AF_UNSPEC, SOCK_STREAM, protocol);
			if(server < 0) {
				*err_msg = "unable to create socket";
				return server;
			}
			}
	}

	/////////////////////////////

	if(protocol == IPPROTO_TCP){
		fprintf(stdout, "Listening on TCP port %s\n", port);
	}
	else if(protocol == IPPROTO_SCTP){
		fprintf(stdout, "Listening on SCTP port %s\n", port);
	}
	// man 7 ip. no importa reportar nada si falla.
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

	if(ip==AF_INET6){
		printf("About to bind IPV6....\n");
		if((ans = bind(server, (struct sockaddr*) &addr_ipv6, sizeof(addr_ipv6))) < 0) {
			*err_msg = "unable to bind socket";
			return ans;
		}
	}
	else if(ip==AF_INET){
		printf("About to bind IPV4....\n");
		if((ans = bind(server, (struct sockaddr*) &addr, sizeof(addr))) < 0) {
			*err_msg = "unable to bind socket";
			return ans;
		}
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
	char 		proxy_address[100];
	char 		proxy_address_ipv6[100];
	char 		client_port[100];
	char 		admin_address[100];
	char 		admin_address_ipv6[100];
	char 		admin_port[100];
	char 		origin_address[100];
	char 		origin_port[100];


	strcpy(proxy_address, "0.0.0.0");
	strcpy(proxy_address_ipv6, "::1");
	strcpy(client_port, "1110");
	strcpy(admin_address, "127.0.0.1");
	strcpy(admin_address_ipv6, "::1");
	strcpy(admin_port, "9090");
	strcpy(origin_address, "127.0.0.1");
	strcpy(origin_port, "110");
	//201.212.7.96

	if((ret = param_validation(argc, argv, proxy_address, proxy_address_ipv6, client_port, admin_address, admin_address_ipv6, admin_port, origin_address, origin_port)) != 0){
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
	const char	*err_msg 	= NULL;
	const int 	server 	 	= init_socket(client_port, proxy_address, &err_msg, IPPROTO_TCP,0);
	const int 	server_ipv6 = init_socket(client_port, proxy_address_ipv6, &err_msg, IPPROTO_TCP,0);


	//printf("Despues de init socket, server tiene el valor de: %d\n",server );
	if(server < 0){
		//printf("server menor a cero\n" );
		goto finally;
	}

	//printf("casi admin\n" );
	const int 	admin 	 	= init_socket(admin_port, admin_address, &err_msg, IPPROTO_SCTP,0);
	const int 	admin_ipv6 	= init_socket(admin_port, admin_address_ipv6, &err_msg, IPPROTO_SCTP,0);
  //Valor 1
	if(admin < 0){
		goto finally;
	}

	const int 	origin 	 = init_socket(origin_port, origin_address, &err_msg, IPPROTO_SCTP,1);
	//Valor 1
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
