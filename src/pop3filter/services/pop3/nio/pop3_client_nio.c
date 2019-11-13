/**
 * socks5nio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
 */
#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>

#include <arpa/inet.h>

#include "../../include/buffer.h"

#include "../../include/stm.h"
#include "include/pop3_client_nio.h"
#include "include/pop3_admin_nio.h"
#include "../../../utils/include/netutils.h"
#include "../parsers/include/pop3_command_parser.h"
#include "../parsers/include/pop3_singleline_response_parser.h"
#include "../parsers/include/pop3_multiline_response_parser.h"
#include "../include/pop3.h"
#include "../../../../services/services.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

#define GREETING_OK "+OK\r\n"

extern struct server_credentials curr_origin_server = {
	.dest_addr_type = req_addrtype_domain, 
	.dest_addr = {
		.fqdn = "pop.fibertel.com.ar"
	},
	.dest_port = 28160,
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

enum states {

	PROCESS_CONNECTION,
	RESOLV_CONNECTION,
	ESTABL_CONNECTION,
	GREETING_SREAD,

	/**
	 * Receives GREETING message from server and process it.
	 *
	 * Interests:
	 *     - OP_READ from origin_fd
	 *
	 * Transitions:
	 *   - GREETING_SREAD		while message has not been completely read
	 *   - GREETING_CWRITE		after being read from server
	 *   - ERROR 				if there is any problem (IO/parsing)
	 *
	GREETING_SREAD,*/

	/**
	 * Sends GREETING to the client
	 *
	 * Interests:
	 *     - OP_WRITE to client_fd
	 *
	 * Transitions:
	 *   - GREETING_CWRITE		while GREETING has not been completely writen
	 *   - COMMAND_CREAD		after being writen to client
	 *   - ERROR				if there is any problem (IO/parsing)
	 */
	GREETING_CWRITE,

	/**
	 * Receives a COMMAND from the client.
	 *
	 * Intereses:
	 *     - OP_READ from client_fd
	 *
	 * Transiciones:
	 *   - COMMAND_CREAD		while COMMAND has not been completely read
	 *   - COMMAND_SWRITE		after being read from the client
	 *   - ERROR				if there is any problem (IO/parsing)
	 */
	COMMAND_CREAD,

	/**
	 * Sends COMMAND to the server
	 *
	 * Interests:
	 *     - OP_WRITE to origin_fd
	 *
	 * Transitions:
	 *   - COMMAND_SWRITE		while COMMAND has not been completely writen
	 *   - RESPONSE_SREAD		after being writen to client
	 *   - ERROR				if there is any problem (IO/parsing)
	 */
	COMMAND_SWRITE,

	/**
	 * Receives a COMMAND from the client.
	 *
	 * Intereses:
	 *     - OP_READ from client_fd
	 *
	 * Transiciones:
	 *   - COMMAND_CREAD		while COMMAND has not been completely read
	 *   - COMMAND_SWRITE		after being read from the client
	 *   - ERROR				if there is any problem (IO/parsing)
	 */
	RESPONSE_SREAD,
	PARTIAL_RESPONSE_CWRITE,

	/**
	 * Sends COMMAND to the server
	 *
	 * Interests:
	 *     - OP_WRITE to origin_fd
	 *
	 * Transitions:
	 *   - COMMAND_SWRITE		while COMMAND has not been completely writen
	 *   - RESPONSE_SREAD		after being writen to client
	 *   - ERROR				if there is any problem (IO/parsing)
	 */
	RESPONSE_CWRITE,

	// estados terminales
	UPDATE,
	ERROR,
};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado

struct connecting_st {
	buffer     *write_buffer;
	const int  *client_fd;
	const int        *origin_fd;
	struct sockaddr_storage       *origin_addr;
	socklen_t                     *origin_addr_len;
	int                           *origin_domain;
	struct addrinfo            	  *origin_resolution;
	struct server_credentials 		origin_server;
};

/** usado por HELLO_READ, HELLO_WRITE */
struct greeting_st {
	buffer 							*read_buffer, *write_buffer;
	struct parser 					*singleline_parser;
};

/** usado por REQUEST_READ, REQUEST_WRITE, REQUEST_RESOLV */
struct command_st {
	/** buffer utilizado para I/O */
	buffer 							*read_buffer, *write_buffer;
	struct pop3_command_builder     *current_command;

	/** parser */
	struct parser 					*command_parser;

	const int 						*client_fd;
	const int 						*origin_fd;
};

/** usado por REQUEST_CONNECTING */
struct response_st {
	buffer     						*write_buffer, *read_buffer;
	const int  						*client_fd;
	const int        				*origin_fd;
	struct parser 					*singleline_parser;
	struct parser 					*multiline_parser;
	struct pop3_command_builder     *current_command;
	bool 							*is_singleline;

};

struct pop3 {
	/** información del cliente */
	struct sockaddr_storage       client_addr;
	socklen_t                     client_addr_len;
	//pop3_new
	int                           client_fd;

	/** resolución de la dirección del origin server */
	struct sockaddr_storage       origin_addr;
	socklen_t                     origin_addr_len;
	int                           origin_domain;
	//pop3_new
	int                           origin_fd;
	struct addrinfo              *origin_resolution;

	//pop3_new
	struct server_credentials 		origin_server;
	struct pop3_command_builder     current_command;


	/** maquinas de estados */
	//pop3_new
	struct state_machine			stm;
	int sender_pipe[2],receiver_pipe[2];
	/** estados para el client_fd */
	union {
		struct command_st			command;
	} client;

	/** estados para el origin_fd */
	union {
		struct connecting_st		connecting;
		struct greeting_st			greeting;
		struct response_st			response;
	} origin;

	bool 							 is_singleline;

	/** buffers para ser usados read_bdescribe_statesuffer, write_buffer.*/
	//pop3_new
	uint8_t raw_buff_a[2048], raw_buff_b[2048];

	//pop3_new
	buffer read_buffer, write_buffer;
	
	/** cantidad de referencias a este objeto. si es uno se debe destruir */
	unsigned references;

	/** siguiente en el pool */
	struct pop3 *next;
};


/**
 * Pool de `struct socks5', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */

static const unsigned  max_pool  = 50; // tamaño máximo
static unsigned        pool_size = 0;  // tamaño actual
static struct pop3 * pool      = 0;  // pool propiamente dicho

static const struct state_definition *
describe_states(void);

/** crea un nuevo `struct socks5' */
static struct pop3 *
pop3_new(int client_fd) {
	struct pop3 *ret;

	if(pool == NULL) {
		ret = malloc(sizeof(*ret));
	} else {
		ret       = pool;
		pool      = pool->next;
		ret->next = 0;
	}
	if(ret == NULL) {
		goto finally;
	}
	memset(ret, 0x00, sizeof(*ret));

	ret->origin_fd       = -1;
	ret->client_fd       = client_fd;
	ret->client_addr_len = sizeof(ret->client_addr);
	ret->origin_server.dest_addr_type 	= curr_origin_server.dest_addr_type;
	ret->origin_server.dest_addr 		= curr_origin_server.dest_addr;
	ret->origin_server.dest_port 		= curr_origin_server.dest_port;

	ret->stm    .initial   = PROCESS_CONNECTION;
	ret->stm    .max_state = ERROR;
	ret->stm    .states    = describe_states();
	ret->is_singleline 	= true;
	stm_init(&ret->stm);

	buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
	buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

	ret->references = 1;
finally:
	return ret;
}

/** realmente destruye */
static void
destroy_suscription_(struct pop3* s) {
	if(s->origin_resolution != NULL) {
		freeaddrinfo(s->origin_resolution);
		s->origin_resolution = 0;
	}
	free(s);
}

/**
 * destruye un  `struct socks5', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
destroy_suscription(struct pop3 *s) {
	if(s == NULL) {
		// nada para hacer
	} else if(s->references == 1) {
		if(s != NULL) {
			if(pool_size < max_pool) {
				s->next = pool;
				pool    = s;
				pool_size++;
			} else {
				destroy_suscription_(s);
			}
		}
	} else {
		s->references -= 1;
	}
}

void
destroy_suscription_pool(void) {
	struct pop3 *next, *s;
	for(s = pool; s != NULL ; s = next) {
		next = s->next;
		free(s);
	}
}

/** obtiene el struct (socks5 *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3 *)(key)->data)

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void read_suscription   (struct selector_key *key);
static void write_suscription  (struct selector_key *key);
static void block_suscription  (struct selector_key *key);
static void close_suscription  (struct selector_key *key);
static const struct fd_handler suscriptions_handler = {
	.handle_read   = read_suscription,
	.handle_write  = write_suscription,
	.handle_close  = close_suscription,
	.handle_block  = block_suscription,
};

static void
process_connection_init(const unsigned state, struct selector_key* key);

static unsigned
process_connection(struct selector_key* key);

/** Attemps to accept an incomming connection */
void
pop3_passive_accept(struct selector_key *key) {
	struct sockaddr_storage       client_addr;
	socklen_t                     client_addr_len = sizeof(client_addr);
	struct pop3              	  *state           = NULL;

	const int client = accept(key->fd, (struct sockaddr*) &client_addr,
														  &client_addr_len);
	if(client == -1) {
		goto fail;
	}
	if(selector_fd_set_nio(client) == -1) {
		goto fail;
	}

	state = pop3_new(client);
	if(state == NULL) {
		// sin un estado, nos es imposible manejaro.
		// tal vez deberiamos apagar accept() hasta que detectemos
		// que se liberó alguna conexión.
		goto fail;
	}
	memcpy(&state->client_addr, &client_addr, client_addr_len);
	state->client_addr_len = client_addr_len;
	
	if(SELECTOR_SUCCESS != selector_register(key->s, client, &suscriptions_handler,
											  OP_WRITE, state)) {
		goto fail;
	} else {
		/*printf("How\n");
		process_connection_init(0, key);
		printf("Did\n");
		state->stm.initial		= process_connection(key);
		printf("This\n");*/
	}
	return ;
fail:
	if(client != -1) {
		close(client);
	}
	destroy_suscription(state);
}

static unsigned
attempt_connection(struct selector_key *, struct connecting_st *);

static void *
resolv_connection_blocking(void *);

/**
 * Procesa el mensaje de tipo `request'.
 * Únicamente soportamos el comando cmd_connect.
 *
 * Si tenemos la dirección IP intentamos establecer la conexión.
 *
 * Si tenemos que resolver el nombre (operación bloqueante) disparamos
 * la resolución en un thread que luego notificará al selector que ha terminado.
 *
 */
static void
process_connection_init(const unsigned state, struct selector_key *key) {
	struct connecting_st * d = &ATTACHMENT(key)->origin.connecting;
	//d->status          = status_general_SOCKS_server_failure;
	//request_parser_init(&d->parser);
	printf("? %d\n", &ATTACHMENT(key)->client_fd);
	d->client_fd       = &ATTACHMENT(key)->client_fd;
	d->origin_fd       = &ATTACHMENT(key)->origin_fd;
	printf("!\n");
	d->origin_server.dest_addr_type 	= curr_origin_server.dest_addr_type;
	d->origin_server.dest_addr 		= curr_origin_server.dest_addr;
	d->origin_server.dest_port 		= curr_origin_server.dest_port;
	printf(":P\n");
	d->origin_addr     = &ATTACHMENT(key)->origin_addr;
	d->origin_addr_len = &ATTACHMENT(key)->origin_addr_len;
	d->origin_domain   = &ATTACHMENT(key)->origin_domain;
}

static unsigned
process_connection(struct selector_key* key) {
	
	struct connecting_st * d = &ATTACHMENT(key)->origin.connecting;

	unsigned  ret;
	pthread_t tid;

	// esto mejoraría enormemente de haber usado
	// sockaddr_storage en el request

	switch (d->origin_server.dest_addr_type) {
		case req_addrtype_ipv4: {
			ATTACHMENT(key)->origin_domain = AF_INET;
			d->origin_server.dest_addr.ipv4.sin_port = d->origin_server.dest_port;
			ATTACHMENT(key)->origin_addr_len =
					sizeof(d->origin_server.dest_addr.ipv4);
			memcpy(&ATTACHMENT(key)->origin_addr, &d->origin_server.dest_addr,
					sizeof(d->origin_server.dest_addr.ipv4));
			ret = attempt_connection(key, d);
			break;

		} case req_addrtype_ipv6: {
			ATTACHMENT(key)->origin_domain = AF_INET6;
			d->origin_server.dest_addr.ipv6.sin6_port = d->origin_server.dest_port;
			ATTACHMENT(key)->origin_addr_len =
					sizeof(d->origin_server.dest_addr.ipv6);
			memcpy(&ATTACHMENT(key)->origin_addr, &d->origin_server.dest_addr,
					sizeof(d->origin_server.dest_addr.ipv6));
			ret = attempt_connection(key, d);
			break;

		} case req_addrtype_domain: {
			struct selector_key* k = malloc(sizeof(*key));
			if(k == NULL) {
				ret       = GREETING_CWRITE;
				//d->status = status_general_SOCKS_server_failure;
				selector_set_interest_key(key, OP_WRITE);
			} else {
				memcpy(k, key, sizeof(*k));
				if(-1 == pthread_create(&tid, 0,
								resolv_connection_blocking, k)) {
					ret       = GREETING_CWRITE;
					//d->status = status_general_SOCKS_server_failure;
					selector_set_interest_key(key, OP_WRITE);
				} else{
					ret = RESOLV_CONNECTION;
					selector_set_interest_key(key, OP_NOOP);
				}
			}
			break;

		} default: {
			ret       = GREETING_CWRITE;
			//d->status = status_address_type_not_supported;
			selector_set_interest_key(key, OP_WRITE);
		}
	}

	return ret;
}

/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void *
resolv_connection_blocking(void *data) {
	struct selector_key *key = (struct selector_key *) data;
	struct pop3       *s   = ATTACHMENT(key);

	pthread_detach(pthread_self());
	s->origin_resolution = 0;
	struct addrinfo hints = {
		.ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
		.ai_socktype  = SOCK_STREAM,  /* Datagram socsocksv5ket */
		.ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
		.ai_protocol  = 0,            /* Any protocol */
		.ai_canonname = NULL,
		.ai_addr      = NULL,
		.ai_next      = NULL,
	};

	char buff[7];
	snprintf(buff, sizeof(buff), "%d",
			 ntohs(s->origin.connecting.origin_server.dest_port));

	getaddrinfo(s->origin.connecting.origin_server.dest_addr.fqdn, buff, &hints,
			   &s->origin_resolution);

	selector_notify_block(key->s, key->fd);

	free(data);

	return 0;
}

/** procesa el resultado de la resolución de nombres */
static unsigned
resolv_connection(struct selector_key *key) {
	struct connecting_st * d = &ATTACHMENT(key)->origin.connecting;
	struct pop3 *s      =  ATTACHMENT(key);

	if(s->origin_resolution == 0) {
		//d->status = status_general_SOCKS_server_failure;
	} else {
		s->origin_domain   = s->origin_resolution->ai_family;
		s->origin_addr_len = s->origin_resolution->ai_addrlen;
		memcpy(&s->origin_addr,
				s->origin_resolution->ai_addr,
				s->origin_resolution->ai_addrlen);
		freeaddrinfo(s->origin_resolution);
		s->origin_resolution = 0;
	}

	return attempt_connection(key, d);
}

/** intenta establecer una conexión con el origin server */
static unsigned
attempt_connection(struct selector_key *key, struct connecting_st *d) {
	bool error                  = false;
	// da legibilidad
	//enum socks_response_status status =  d->status;
	int *fd                           =  d->origin_fd;

	*fd = socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);
	if (*fd == -1) {
		error = true;
		goto finally;
	}
	if (selector_fd_set_nio(*fd) == -1) {
		goto finally;
	}
	if (-1 == connect(*fd, (const struct sockaddr *)&ATTACHMENT(key)->origin_addr,
						   ATTACHMENT(key)->origin_addr_len)) {
		if(errno == EINPROGRESS) {
			// es esperable,  tenemos que esperar a la conexión

			// dejamos de de pollear el socket del cliente
			selector_status st = selector_set_interest_key(key, OP_NOOP);
			if(SELECTOR_SUCCESS != st) {
				error = true;
				goto finally;
			}

			// esperamos la conexion en el nuevo socket
			st = selector_register(key->s, *fd, &suscriptions_handler,
									  OP_WRITE, key->data);
			if(SELECTOR_SUCCESS != st) {
				error = true;
				goto finally;
			}
			ATTACHMENT(key)->references += 1;
		} else {
			//status = errno_to_socks(errno);
			error = true;
			goto finally;
		}
	} else {
		// estamos conectados sin esperar... no parece posible
		// saltaríamos directamente a COPY
		abort();
	}

finally:
	if (error) {
		if (*fd != -1) {
			close(*fd);
			*fd = -1;
		}
	}

	//d->status = status;

	return ESTABL_CONNECTION;
}

/*static void
request_read_close(const unsigned state, struct selector_key *key) {
	struct request_st * d = &ATTACHMENT(key)->client.request;

	request_close(&d->parser);
}*/

////////////////////////////////////////////////////////////////////////////////
// REQUEST CONNECT
////////////////////////////////////////////////////////////////////////////////
static void
establ_connection_init(const unsigned state, struct selector_key *key) {
	struct connecting_st *d  = &ATTACHMENT(key)->origin.connecting;

	d->client_fd = &ATTACHMENT(key)->client_fd;
	d->origin_fd = &ATTACHMENT(key)->origin_fd;
	//d->status   			= &ATTACHMENT(key)->client.request.status;
	d->write_buffer			= &ATTACHMENT(key)->write_buffer;
}

/** la conexión ha sido establecida (o falló)  */
static unsigned
establ_connection(struct selector_key *key) {
	
	int error;
	socklen_t len = sizeof(error);
	struct connecting_st *d  = &ATTACHMENT(key)->origin.connecting;

	if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		//*d->status = status_general_SOCKS_server_failure;
	} else {
		if(error == 0) {
			//*d->status     = status_succeeded;
			d->origin_fd = &key->fd;
		} else {
			//*d->status = errno_to_socks(error);
		}
	}

	/*if(-1 == request_marshall(d->write_buffer, *d->status)) {
		*d->status = status_general_SOCKS_server_failure;
		abort(); // el buffer tiene que ser mas grande en la variable
	}*/
	selector_status s = 0;
	s |= selector_set_interest    (key->s, *(d->client_fd), OP_NOOP);
	s |= selector_set_interest_key(key,                   OP_READ);

	return SELECTOR_SUCCESS == s ? GREETING_SREAD : ERROR;
}

/** inicializa las variables de los estados HELLO_… */
static void
greeting_init(const unsigned state, struct selector_key *key) {
	struct greeting_st *d	= &ATTACHMENT(key)->origin.greeting;

	/**
	 * pop3.client.greeting.write_buffer is a buffer_ptr and thus we make it point to a real buffer
	 *
	 */

	d->write_buffer 		= &(ATTACHMENT(key)->write_buffer);
	d->read_buffer			= &(ATTACHMENT(key)->read_buffer);

	/**
	 * The only buffer we will be using is the write_buffer which we'll preload
	 * with the greeting message.
	 *
	 * R=0
	 * ↓
	 * +---+---+---+----+----+----+
	 * | + | O | K | \r | \n | \0 |
	 * +---+---+---+----+----+----+
	 * ↑                     ↑
	 * W=0                limit=sizeof(GREETING_OK)-1
	 *
	 * Remember that strings terminate on \0 and this should not be sent (thus the size-1 restriction).
	 * Also the \0 is NOT inside the buffer.
	 *
	 */

	buffer_init(d->write_buffer, sizeof(GREETING_OK)-1, &GREETING_OK);

	/**
	 * Now we shall advance the write_ptr to match the limit, so the string can be read.
	 *
	 * R=0
	 * ↓
	 * +---+---+---+----+----+
	 * | + | O | K | \r | \n |
	 * +---+---+---+----+----+
	 *                       ↑
	 *                      W=limit=sizeof(GREETING_OK)-1
	 *
	 */

	buffer_write_adv(d->write_buffer, sizeof(GREETING_OK)-1);

	d->singleline_parser = pop3_singleline_response_parser_init();
}

void breakpoint(void){

}

static unsigned
greeting_sread(struct selector_key *key) {
	printf("We did it Charizard\n");
	//exit(0);
	struct greeting_st *d = &ATTACHMENT(key)->origin.greeting;
	unsigned  						 ret      = GREETING_SREAD;
		bool  						 error    = false;
	 uint8_t 									*ptr;
	  size_t  						 			count;
	 ssize_t  						 			n;
	 enum structure_builder_states 	 			state;
	 struct pop3_singleline_response_builder 	builder;


	ptr = buffer_write_ptr(d->read_buffer, &count);

	n = recv(ATTACHMENT(key)->origin_fd, ptr, count, 0);
	printf("%d %ld %ld %s\n", ATTACHMENT(key)->origin_fd, count, n, errno == EAGAIN ? "No hay nada" : "Mal");
	printf("Hola, dame bola\n");
	if(n > 0) {
		printf("%s\n", ptr);
		printf("Hola, dame bola\n");
		buffer_write_adv(d->read_buffer, n);
		state = pop3_singleline_response_builder(d->read_buffer, d->singleline_parser, &builder, &error);
		printf("Haremos lo necesitamos para salvar esta compania\n");
		//struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, d->singleline_parser, &error);
		selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
		if(state == BUILD_FINISHED && SELECTOR_SUCCESS == selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
			ret = GREETING_CWRITE;
		} else {
			ret = ERROR;
		}
		printf("Need rest\n");
		breakpoint();
		/*if(pop3_singleline_parser_is_done(event, &error)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE) && !error) {
				ret = GREETING_CWRITE;
			} else {
				ret = ERROR;
			}
		}*/
	} else {
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			errno = 0;
			ret = GREETING_SREAD;
		} else {
			//ret = ERROR;
			ret = GREETING_CWRITE;
		}
	}
	printf("Stressed out: %s\n", ret == ERROR ? "error" : (ret == GREETING_CWRITE ? "estamos bien" : "no estamos bien"));

	return error ? ERROR : ret;
}

/** Writes the GREETING to the client */
static unsigned
greeting_cwrite(struct selector_key *key) {
	printf("Hi\n");
	fflush(stdout);
	struct greeting_st *d = &ATTACHMENT(key)->origin.greeting;

	unsigned  ret     = GREETING_CWRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_read_ptr(d->write_buffer, &count);
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		buffer_read_adv(d->write_buffer, n);
		if(!buffer_can_read(d->write_buffer)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				ret = COMMAND_CREAD;
			} else {
				ret = ERROR;
			}
		}
	}
	printf("Yeah\n");

	return ret;
}

static void
command_init(const unsigned state, struct selector_key *key) {
	struct command_st * d = &ATTACHMENT(key)->client.command;

	d->read_buffer              = &(ATTACHMENT(key)->read_buffer);
	d->write_buffer             = &(ATTACHMENT(key)->write_buffer);
	buffer_init(d->write_buffer, N(ATTACHMENT(key)->raw_buff_b), ATTACHMENT(key)->raw_buff_b);
	d->command_parser 			= pop3_command_parser_init();
	d->current_command 			= &(ATTACHMENT(key)->current_command);
	*(d->current_command) 		= pop3_command_builder_default;
	buffer_reset(d->read_buffer);
	buffer_reset(d->write_buffer);
}

static unsigned
command_cread(struct selector_key *key) {
	printf("Hola\n");
	struct command_st *d = &ATTACHMENT(key)->client.command;
	unsigned  ret      = COMMAND_CREAD;
		bool  error    = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	 enum structure_builder_states 	 			state;

	ptr = buffer_write_ptr(d->read_buffer, &count);
	printf("%Hakuna %d\n", count);
	n = recv(key->fd, ptr, count, 0);
	printf("BYe %d\n", n);
	if(n > 0) {
		printf("Thats my secret hulk, %s\n", ptr);
		buffer_write_adv(d->read_buffer, n);
		printf("%BRUJERIAS %d\n", d->read_buffer->write);
		printf("Heyo\n");
		state = command_builder(d->read_buffer, d->command_parser, d->current_command, &error);
		printf("Wiiiii %d\n", d->read_buffer->write);
		fflush(stdout);
		
		if(state == BUILD_FINISHED ){
			d->read_buffer->read = d->read_buffer->data;
			selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
			if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE)){
				printf("Thats my secret cap, %s %s\n", d->current_command->kwrd, ptr);
				ret = COMMAND_SWRITE;
			} else {
				ret = ERROR;
			}
			
		} else if(state == BUILDING){
			//selector_set_interest_key(key, OP_WRITE);
			printf("Ship\n");
			ret = COMMAND_CREAD;
		} else {
			printf("Why???\n");
			ret = ERROR;
		}
		/*const struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, &d->singleline_parser, &error);
		if(pop3_command_parser_is_done(event, &error)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE) && !error) {
				ret = COMMAND_SWRITE;
			} else {
				ret = ERROR;
			}
		}*/
	} else {
		ret = ERROR;
	}
	printf("ksmdklsmcdks\n");
	return error ? ERROR : ret;
}

static unsigned
command_swrite(struct selector_key *key) {
	printf("Mehhh\n");
	struct command_st *d = &ATTACHMENT(key)->client.command;
	
	unsigned  ret     = COMMAND_SWRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_read_ptr(d->read_buffer, &count);
	printf("aksnkas %s %d %d\n", ptr, count, d->read_buffer->write);
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	printf("Alaska %s %d\n", ptr, n);
	if(n == -1) {
		ret = ERROR;
	} else {
		buffer_read_adv(d->read_buffer, n);
		if(!buffer_can_read(d->read_buffer)) {
			printf("Mimuuu %d\n", ATTACHMENT(key)->origin_fd);
			//key->fd = ATTACHMENT(key)->origin_fd;
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				printf("Manu Luque\n");
				ret = RESPONSE_SREAD;
			} else {
				printf("Im fucked\n");
				ret = ERROR;
			}
		}
	}
	printf("My my\n");

	return ret;
}

/** inicializa las variables de los estados HELLO_… */
static void
response_init(const unsigned state, struct selector_key *key) {
	printf("Sadness\n");
	struct response_st *d 		= &ATTACHMENT(key)->origin.response;
	d->current_command 			= &(ATTACHMENT(key)->current_command);
	d->singleline_parser 		= pop3_singleline_response_parser_init();
	d->multiline_parser 		= pop3_multiline_response_parser_init();
	printf("Puntero %p\n", d->multiline_parser);
	d->is_singleline 			= &ATTACHMENT(key)->is_singleline;
	buffer_init(d->write_buffer,N(ATTACHMENT(key)->raw_buff_b),ATTACHMENT(key)->raw_buff_b);
	buffer_init(d->read_buffer,N(ATTACHMENT(key)->raw_buff_a),ATTACHMENT(key)->raw_buff_a);	
	buffer_reset(d->read_buffer);
	buffer_reset(d->write_buffer);
}

static unsigned
response_sread(struct selector_key *key) {
	printf("Im still here\n");
	fflush(stdout);
	struct response_st *d 	= &ATTACHMENT(key)->origin.response;
	unsigned  ret      		= RESPONSE_SREAD;
		bool  error    		= false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	  enum structure_builder_states 	 			state;
	 struct pop3_singleline_response_builder 		builder;
	fflush(stdout);
	ptr = buffer_write_ptr(d->write_buffer, &count);
	n = recv(key->fd, ptr, count, 0);
	fflush(stdout);
	if(n > 0) {
		buffer_write_adv(d->write_buffer, n);
		if(*(d->is_singleline)){
			state = pop3_singleline_response_builder(d->write_buffer, d->singleline_parser, &builder, &error);
			//buffer_read_adv(d->write_buffer, n);
			//struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, d->singleline_parser, &error);
			
			if(state == BUILD_FINISHED){
				enum pop3_command_types type = get_command_type(d->current_command->kwrd);
				switch(type){
					case OVERLOADED:
						printf("OVERLOADED\n");
						if(!d->current_command->has_args){
							break;
						}
					case MULTILINE:
						*(d->is_singleline) = false;
						printf("MULTILINE\n");
						enum consumer_state c_state = pop3_multiline_response_checker(d->write_buffer, d->multiline_parser, &error);
						if(strcmp(d->current_command->kwrd, "RETR") == 0 || strcmp(d->current_command->kwrd, "TOP") == 0){
							int bytes_to_read;
							uint8_t *ptr = buffer_read_ptr(d->write_buffer, &bytes_to_read);
							int resp = create_transformation(ATTACHMENT(key)->sender_pipe, ATTACHMENT(key)->receiver_pipe);
							printf("My little bunny %d\n", resp);
							write(ATTACHMENT(key)->sender_pipe[0], ptr, bytes_to_read);
							printf("Chiguagua\n");
							uint8_t *read_ptr = buffer_write_ptr(d->read_buffer, &bytes_to_read);
							read(ATTACHMENT(key)->receiver_pipe[1], read_ptr, bytes_to_read);
							if(c_state != FINISHED_CONSUMING ){
								return RESPONSE_SREAD;
							} else {
								if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
									ret = RESPONSE_CWRITE;
								} else {
									ret = ERROR;
								}
								return ret;
							}
						} else {

						}
						if(c_state == FINISHED_CONSUMING ){
							selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
							printf("Hashtag\n");
							if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
								ret = RESPONSE_CWRITE;
							} else {
								ret = ERROR;
							}
							return ret;
						} else {
							selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
							if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
								ret = PARTIAL_RESPONSE_CWRITE;
							} else {
								ret = ERROR;
							}
							return ret;
						}

				}
				selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
				if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
					ret = RESPONSE_CWRITE;
				} else {
					ret = ERROR;
				}
				
			} else {
				ret = ERROR;
			}
		} else {
			enum consumer_state c_state = pop3_multiline_response_checker(d->write_buffer, d->multiline_parser, &error);
			if(strcmp(d->current_command->kwrd, "RETR") == 0 || strcmp(d->current_command->kwrd, "TOP") == 0){
				int bytes_to_read;
				uint8_t *ptr=buffer_read_ptr(d->write_buffer,&bytes_to_read);
				write(ATTACHMENT(key)->sender_pipe[0],ptr,bytes_to_read);
			} else {
			}
			if(c_state == FINISHED_CONSUMING ){
				selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
				if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
					ret = RESPONSE_CWRITE;
				} else {
					ret = ERROR;
				}
				return ret;
			} else {
				selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
				if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
					ret = PARTIAL_RESPONSE_CWRITE;
				} else {
					ret = ERROR;
				}
			}
		}
		/*const struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, &d->singleline_parser, &error);
		if(pop3_singleline_parser_is_done(event, 0)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
				ret = RESPONSE_CWRITE;
			} else {
				ret = ERROR;
			}
		}*/
	} else {
		ret = ERROR;
	}

	return error ? ERROR : ret;
}

static unsigned
partial_response_cwrite(struct selector_key *key) {
	struct response_st *d = &ATTACHMENT(key)->origin.response;

	unsigned  ret     = PARTIAL_RESPONSE_CWRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_read_ptr(d->read_buffer, &count);
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		buffer_read_adv(d->read_buffer, n);
		if(!buffer_can_read(d->read_buffer)) {
			selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
			if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ)){
				ret = RESPONSE_SREAD;
			} else {
				ret = ERROR;
			}
		}
	}

	return ret;
}

static unsigned
response_cwrite(struct selector_key *key) {
	struct response_st *d = &ATTACHMENT(key)->origin.response;

	unsigned  ret     = RESPONSE_CWRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_read_ptr(d->write_buffer, &count);
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		buffer_read_adv(d->write_buffer, n);
		if(!buffer_can_read(d->write_buffer)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				ret = COMMAND_CREAD;
			} else {
				ret = ERROR;
			}
		}
	}

	return ret;
}

/** Handler definition for each state */
static const struct state_definition client_statbl[] = {
	{
		.state 				= PROCESS_CONNECTION,
		.on_arrival 		= process_connection_init,
		.on_write_ready		= process_connection,
	},{
		.state 				= RESOLV_CONNECTION,
		.on_block_ready 	= resolv_connection,
	},{
		.state 				= ESTABL_CONNECTION,
		.on_arrival 		= establ_connection_init,
		.on_write_ready 	= establ_connection,
	},{
		.state 				= GREETING_SREAD,
		.on_arrival			= greeting_init,
		.on_read_ready		= greeting_sread,
	},{
		.state				= GREETING_CWRITE,
		.on_write_ready		= greeting_cwrite,
	},{
		.state				= COMMAND_CREAD,
		.on_arrival			= command_init,
		.on_read_ready		= command_cread,
	},{
		.state				= COMMAND_SWRITE,
		.on_write_ready		= command_swrite,
	},{
		.state				= RESPONSE_SREAD,
		.on_arrival 		= response_init,
		.on_read_ready		= response_sread,
	},{
		.state				= PARTIAL_RESPONSE_CWRITE,
		.on_write_ready		= partial_response_cwrite,
	},{
		.state				= RESPONSE_CWRITE,
		.on_write_ready		= response_cwrite,
	},{
		.state				= UPDATE,

	},{
		.state				= ERROR,
	}
};

static const struct state_definition *
describe_states(void) {
	return client_statbl;
}

///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.
static void
finished_suscription(struct selector_key* key);

static void
read_suscription(struct selector_key *key) {
	struct state_machine *stm   = &ATTACHMENT(key)->stm;
	const enum states st = stm_handler_read(stm, key);

	if(ERROR == st || UPDATE == st) {
		finished_suscription(key);
	}
}

static void
write_suscription(struct selector_key *key) {
	struct state_machine *stm   = &ATTACHMENT(key)->stm;
	const enum states st = stm_handler_write(stm, key);

	if(ERROR == st || UPDATE == st) {
		finished_suscription(key);
	}
}

static void
block_suscription(struct selector_key *key) {
	struct state_machine *stm   = &ATTACHMENT(key)->stm;
	const enum states st = stm_handler_block(stm, key);

	if(ERROR == st || UPDATE == st) {
		finished_suscription(key);
	}
}

static void
close_suscription(struct selector_key *key) {
	destroy_suscription(ATTACHMENT(key));
}

static void
finished_suscription(struct selector_key* key) {
	const int fds[] = {
		ATTACHMENT(key)->client_fd,
		ATTACHMENT(key)->origin_fd,
	};
	for(unsigned i = 0; i < N(fds); i++) {
		if(fds[i] != -1) {
			if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
				abort();
			}
			close(fds[i]);
		}
	}
}
