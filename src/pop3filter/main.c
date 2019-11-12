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
param_validation(const int argc, const char **argv, char* proxy_port, char *origin_port, char* proxy_address){
	command_line_parser(argc, argv, proxy_port, origin_port, proxy_address);
	return 0;
}

static 
int 
init_socket(const char * proxy_port, const char * proxy_address, const char **err_msg, int protocol){
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = inet_addr(proxy_address);
	addr.sin_port        = htons(atoi(proxy_port));

	int ans;

	const int server = socket(AF_INET, SOCK_STREAM, protocol);
	if(server < 0) {
		*err_msg = "unable to create socket";
		return server;
	}

	fprintf(stdout, "Listening on TCP port %s\n", proxy_port);

	// man 7 ip. no importa reportar nada si falla.
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

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
	char 		proxy_port[100];
	char 		origin_port[100];
	char 		proxy_address[100];

	strcpy(proxy_port, "1100");
	strcpy(origin_port, "110");
	strcpy(proxy_address, "0.0.0.0");

	if((ret = param_validation(argc, argv, proxy_port, origin_port, proxy_address)) != 0){	
		return ret;
	}


	// No input from user is needed
	close(STDIN_FILENO);

	// Variable for error messages
	const char	*err_msg = NULL;
	const int 	server 	 = init_socket(proxy_port, proxy_address, &err_msg, IPPROTO_TCP);

	if(server < 0){
		goto finally;
	}

	char 		admin_address[100];
	strcpy(admin_address, "0.0.0.0");

	const int 	admin 	 = init_socket("9090", admin_address, &err_msg, IPPROTO_SCTP);

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