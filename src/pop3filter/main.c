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
#include "../include/lib.h"
#include "services/pop3/include/pop3_suscriptor.h"

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
param_validation(const int argc, const char **argv, unsigned *port){
	if(argc == 1) {
		return 0;
		// utilizamos el default
	} else if(argc == 2) {
		char *end     = 0;
		const long sl = strtol(argv[1], &end, 10);

		if (end == argv[1]|| '\0' != *end
		   || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
		   || sl < 0 || sl > USHRT_MAX) {
			fprintf(stderr, "port should be an integer: %s\n", argv[1]);
			return 1;
		}
		*port = sl;
	} else {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		return 1;
	}
	return command_line_parser(argc,argv,10,port);
}

static
int
init_socket(const unsigned port, const char **err_msg){
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons(port);

	int ans;

	const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if(server < 0) {
		*err_msg = "unable to create socket";
		return server;
	}

	fprintf(stdout, "Listening on SCTP port %d\n", port);

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
	unsigned 	port 	 = 1080;

	if((ret = param_validation(argc, argv, &port)) != 0){
		printf("%d",ret);
		return ret;
	}

	// No input from user is needed
	close(STDIN_FILENO);

	// Variable for error messages
	const char	*err_msg = NULL;
	const int 	server 	 = init_socket(port, &err_msg);

	if(server < 0){
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

	selector_status ss = init_suscription_service(server, &err_msg);

finally:

	ret = print_err_msg(ss, &err_msg);

	if(server >= 0) {
		close(server);
	}
	return ret;
}
