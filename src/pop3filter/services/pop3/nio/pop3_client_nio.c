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

#include "buffer.h"

#include "stm.h"
#include "pop3_nio.h"
#include"netutils.h"

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
	struct addrinfo              *origin_resolution;
	struct server_credentials 		origin_server;
};

/** usado por HELLO_READ, HELLO_WRITE */
struct greeting_st {
	buffer 					*read_buffer, *write_buffer;
	struct parser 			*singleline_parser;
};

/** usado por REQUEST_READ, REQUEST_WRITE, REQUEST_RESOLV */
struct command_st {
	/** buffer utilizado para I/O */
	buffer 					*read_buffer, *write_buffer;

	/** parser */
	struct parser 			*command_parser;
	struct parser 			*singleline_parser;

	const int 				*client_fd;
	const int 					*origin_fd;
};

/** usado por REQUEST_CONNECTING */
struct response_st {
	buffer     *write_buffer, *read_buffer;
	const int  	*client_fd;
	const int        *origin_fd;
	struct parser 			*singleline_parser;

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


	/** maquinas de estados */
	//pop3_new
	struct state_machine			stm;

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

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado

/** usado por HELLO_READ, HELLO_WRITE *
struct hello_st {
	/** buffer utilizado para I/O *
	buffer               *read_buffer, *write_buffer;
	struct hello_parser   parser;
	/** el método de autenticación seleccionado *
	uint8_t               method;
} ;

/** usado por REQUEST_READ, REQUEST_WRITE, REQUEST_RESOLV *
struct request_st {
	/** buffer utilizado para I/O *
	buffer                    *read_buffer, *write_buffer;

	/** parser *
	struct request             request;
	struct request_parser      parser;

	/** el resumen del respuesta a enviar*
	enum socks_response_status status;

	// ¿a donde nos tenemos que conectar?
	struct sockaddr_storage   *origin_addr;
	socklen_t                 *origin_addr_len;
	int                       *origin_domain;

	const int                 *client_fd;
	int                       *origin_fd;
};*/

/** usado por REQUEST_CONNECTING *
struct connecting {
	buffer     *write_buffer;
	const int  *client_fd;
	int        *origin_fd;
	enum socks_response_status *status;
};*/

/** usado por COPY *
struct copy {
	/** el otro file descriptor *
	int         *fd;
	/** el buffer que se utiliza para hacer la copia *
	buffer      *read_buffer, *write_buffer;
	/** ¿cerramos ya la escritura o la lectura? *
	fd_interest duplex;

	struct copy *other;
};*/

/*
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una única
 * alocación cuando recibimos la conexión.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 *
struct socks5 {
	/** información del cliente *
	struct sockaddr_storage       client_addr;
	socklen_t                     client_addr_len;
	int                           client_fd;

	/** resolución de la dirección del origin server *
	struct addrinfo              *origin_resolution;
	/** intento actual de la dirección del origin server *
	struct addrinfo              *origin_resolution_current;

	/** informació
		struct copy               copy;n del origin server *
	struct sockaddr_storage       origin_addr;
	socklen_t                     origin_addr_len;
	int                           origin_domain;
	int                           origin_fd;


	/** maquinas de estados *
	struct state_machine          stm;

	/** estados para el client_fd *
	union {
		struct hello_st           hello;
		struct request_st         request;
		struct copy               copy;
	} client;
	/** estados para el origin_fd *
	union {
		struct connecting         conn;
		struct copy               copy;
	} origin;

	/** buffers para ser usados read_bdescribe_statesuffer, write_buffer.*
	uint8_t raw_buff_a[2048], raw_buff_b[2048];
	buffer read_buffer, write_buffer;
	
	/** cantidad de referencias a este objeto. si es uno se debe destruir *
	unsigned references;

	/** siguiente en el pool *
	struct socks5 *next;
};*/


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
											  OP_READ, state)) {
		goto fail;
	}
	return ;
fail:
	if(client != -1) {
		close(client);
	}
	destroy_suscription(state);
}

/** definición de handlers para cada estado 
static const struct state_definition client_statbl[] = {
	{
		.state            = HELLO_READ,
		.on_arrival       = hello_read_init,
		.on_departure     = hello_read_close,
		.on_read_ready    = hello_read,
	}, {
		.state            = HELLO_WRITE,
		.on_write_ready   = hello_write,
	},{
		.state            = REQUEST_READ,
		.on_arrival       = request_init,
		.on_departure     = request_read_close,
		.on_read_ready    = request_read,
	},{
		.state            = REQUEST_RESOLV,
		.on_block_ready   = request_resolv_done,
	},{
		.state            = REQUEST_CONNECTING,
		.on_arrival       = request_connecting_init,
		.on_write_ready   = request_connecting,
	},{
		.state            = REQUEST_WRITE,
		.on_write_ready   = request_write,
	}, {
		.state            = COPY,
		.on_arrival       = copy_init,
		.on_read_ready    = copy_r,
		.on_write_ready   = copy_w,
	}, {
		.state            = DONE,
              
	},{
		.state            = ERROR,
	}
};*/

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
	d->client_fd       = &ATTACHMENT(key)->client_fd;
	d->origin_fd       = &ATTACHMENT(key)->origin_fd;

	d->origin_server.dest_addr_type 	= curr_origin_server.dest_addr_type;
	d->origin_server.dest_addr 		= curr_origin_server.dest_addr;
	d->origin_server.dest_port 		= curr_origin_server.dest_port;

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
	s |= selector_set_interest    (key->s, *(d->client_fd), OP_READ);
	s |= selector_set_interest_key(key,                   OP_NOOP);

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

	buffer_init(d->write_buffer, sizeof(GREETING_OK)-1, GREETING_OK);

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
	unsigned  ret      = GREETING_SREAD;
		bool  error    = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_write_ptr(d->read_buffer, &count);

	n = recv(ATTACHMENT(key)->origin_fd, ptr, count, 0);
	printf("%ld %ld %s\n", count, n, errno == EAGAIN ? "No hay nada" : "Mal");
	printf("Hola, dame bola\n");
	if(n > 0) {
		printf("Hola, dame bola\n");
		buffer_write_adv(d->read_buffer, n);
		printf("Haremos lo necesitamos para salvar esta compania\n");
		//struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, d->singleline_parser, &error);
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

	return error ? ERROR : ret;
}

/** Writes the GREETING to the client */
static unsigned
greeting_cwrite(struct selector_key *key) {
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

	return ret;
}

static void
command_init(const unsigned state, struct selector_key *key) {
	struct command_st * d = &ATTACHMENT(key)->client.command;

	d->read_buffer              = &(ATTACHMENT(key)->read_buffer);
	d->write_buffer             = &(ATTACHMENT(key)->write_buffer);
}

static unsigned
command_cread(struct selector_key *key) {
	struct command_st *d = &ATTACHMENT(key)->client.command;
	unsigned  ret      = COMMAND_CREAD;
		bool  error    = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_write_ptr(d->read_buffer, &count);
	n = recv(key->fd, ptr, count, 0);
	if(n > 0) {
		buffer_write_adv(d->read_buffer, n);
		const struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, &d->singleline_parser, &error);
		if(pop3_command_parser_is_done(event, &error)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE) && !error) {
				ret = COMMAND_SWRITE;
			} else {
				ret = ERROR;
			}
		}
	} else {
		ret = ERROR;
	}

	return error ? ERROR : ret;
}

static unsigned
command_swrite(struct selector_key *key) {
	struct command_st *d = &ATTACHMENT(key)->client.command;

	unsigned  ret     = COMMAND_SWRITE;
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
				ret = RESPONSE_SREAD;
			} else {
				ret = ERROR;
			}
		}
	}

	return ret;
}

static unsigned
response_sread(struct selector_key *key) {
	struct response_st *d = &ATTACHMENT(key)->origin.response;
	unsigned  ret      = GREETING_SREAD;
		bool  error    = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_write_ptr(d->read_buffer, &count);
	n = recv(key->fd, ptr, count, 0);
	if(n > 0) {
		buffer_write_adv(d->read_buffer, n);
		const struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, &d->singleline_parser, &error);
		if(pop3_singleline_parser_is_done(event, 0)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
				ret = RESPONSE_CWRITE;
			} else {
				ret = ERROR;
			}
		}
	} else {
		ret = ERROR;
	}

	return error ? ERROR : ret;
}

static unsigned
response_cwrite(struct selector_key *key) {
	struct response_st *d = &ATTACHMENT(key)->origin.response;

	unsigned  ret     = COMMAND_SWRITE;
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
		.on_read_ready 		= process_connection,
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
		.on_read_ready		= response_sread,
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

/** usado por COPY *
struct copy {
	/** el otro file descriptor *
	int         *fd;
	/** el buffer que se utiliza para hacer la copia *
	buffer      *read_buffer, *write_buffer;
	/** ¿cerramos ya la escritura o la lectura? *
	fd_interest duplex;

	struct copy *other;
};*/

/*
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una única
 * alocación cuando recibimos la conexión.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */

////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** callback del parser utilizado en `read_hello' */
/*static void
on_hello_method(struct hello_parser *p, const uint8_t method) {
	uint8_t *selected  = p->data;

	if(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED == method) {
	   *selected = method;
	}
}



static unsigned
hello_process(const struct hello_st* d);*/

/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
/*static unsigned
hello_read(struct selector_key *key) {
	struct hello_st *d = &ATTACHMENT(key)->client.hello;
	unsigned  ret      = HELLO_READ;
		bool  error    = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_write_ptr(d->read_buffer, &count);
	n = recv(key->fd, ptr, count, 0);
	if(n > 0) {
		buffer_write_adv(d->read_buffer, n);
		const enum hello_state st = hello_consume(d->read_buffer, &d->parser, &error);
		if(hello_is_done(st, 0)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
				ret = hello_process(d);
			} else {
				ret = ERROR;
			}
		}
	} else {
		ret = ERROR;
	}

	return error ? ERROR : ret;
}*/


/** procesamiento del mensaje `hello' */
/*static unsigned
hello_process(const struct hello_st* d) {
	unsigned ret = HELLO_WRITE;

	uint8_t m = d->method;
	const uint8_t r = (m == SOCKS_HELLO_NO_ACCEPTABLE_METHODS) ? 0xFF : 0x00;
	if (-1 == hello_marshall(d->write_buffer, r)) {
		ret  = ERROR;
	}
	if (SOCKS_HELLO_NO_ACCEPTABLE_METHODS == m) {
		ret  = ERROR;
	}
	return ret;
}*/

/** libera los recursos al salir de HELLO_READ */
/*static void
hello_read_close(const unsigned state, struct selector_key *key) {
	struct hello_st *d = &ATTACHMENT(key)->client.hello;

	hello_parser_close(&d->parser);
}
*/


/** escribe todos los bytes de la respuesta al mensaje `hello' */
/*static unsigned
hello_write(struct selector_key *key) {
	struct hello_st *d = &ATTACHMENT(key)->client.hello;

	unsigned  ret     = HELLO_WRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_read_ptr(d->write_buffer, &count);
	char *greeting = GREETING;
	n = send(key->fd, GREETING, sizeof(GREETING), MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		buffer_read_adv(d->write_buffer, n);
		if(!buffer_can_read(d->write_buffer)) {
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				ret = REQUEST_READ;
			} else {
				ret = ERROR;
			}
		}
	}

	return ret;
}*/

////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables de los estados REQUEST_… */
/*static void
request_init(const unsigned state, struct selector_key *key) {
	struct request_st * d = &ATTACHMENT(key)->client.request;

	d->read_buffer              = &(ATTACHMENT(key)->read_buffer);
	d->write_buffer              = &(ATTACHMENT(key)->write_buffer);
	d->parser.request  = &d->request;
	d->status          = status_general_SOCKS_server_failure;
	request_parser_init(&d->parser);
	d->client_fd       = &ATTACHMENT(key)->client_fd;
	d->origin_fd       = &ATTACHMENT(key)->origin_fd;

	d->origin_addr     = &ATTACHMENT(key)->origin_addr;
	d->origin_addr_len = &ATTACHMENT(key)->origin_addr_len;
	d->origin_domain   = &ATTACHMENT(key)->origin_domain;
}

static unsigned
request_process(struct selector_key* key, struct request_st* d);*/

/** lee todos los bytes del mensaje de tipo `request' y inicia su proceso */
/*static unsigned
request_read(struct selector_key *key) {
	struct request_st * d = &ATTACHMENT(key)->client.request;

	  buffer *b     = d->read_buffer;
	unsigned  ret   = REQUEST_READ;
		bool  error = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_write_ptr(b, &count);
	n = recv(key->fd, ptr, count, 0);
	if(n > 0) {
		buffer_write_adv(b, n);
		int st = request_consume(b, &d->parser, &error);
		if(request_is_done(st, 0)) {
			ret = request_process(key, d);
		}
	} else {
		ret = ERROR;
	}

	return error ? ERROR : ret;
}


static unsigned
request_connect(struct selector_key *key, struct request_st * d);

static void *
resolv_connection_blocking(void *data);*/

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
/*static unsigned
request_process(struct selector_key* key, struct request_st* d) {
	unsigned  ret;REQUEST_READ
	pthread_t tid;

	switch (d->request.cmd) {
		case socks_req_cmd_connect:
			// esto mejoraría enormemente de haber usado
			// sockaddr_storage en el request

			switch (d->request.dest_addr_type) {
				case socks_req_addrtype_ipv4: {
					ATTACHMENT(key)->origin_domain = AF_INET;
					d->request.dest_addr.ipv4.sin_port = d->request.dest_port;
					ATTACHMENT(key)->origin_addr_len =
							sizeof(d->request.dest_addr.ipv4);
					memcpy(&ATTACHMENT(key)->origin_addr, &d->request.dest_addr,
							sizeof(d->request.dest_addr.ipv4));
					ret = request_connect(key, d);
					break;

				} case socks_req_addrtype_ipv6: {
					ATTACHMENT(key)->origin_domain = AF_INET6;
					d->request.dest_addr.ipv6.sin6_port = d->request.dest_port;
					ATTACHMENT(key)->origin_addr_len =
							sizeof(d->request.dest_addr.ipv6);
					memcpy(&ATTACHMENT(key)->origin_addr, &d->request.dest_addr,
							sizeof(d->request.dest_addr.ipv6));
					ret = request_connect(key, d);
					break;

				} case socks_req_addrtype_domain: {
					struct selector_key* k = malloc(sizeof(*key));
					if(k == NULL) {
						ret       = REQUEST_WRITE;
						d->status = status_general_SOCKS_server_failure;
						selector_set_interest_key(key, OP_WRITE);
					} else {
						memcpy(k, key, sizeof(*k));
						if(-1 == pthread_create(&tid, 0,
										resolv_connection_blocking, k)) {
							ret       = REQUEST_WRITE;
							d->status = status_general_SOCKS_server_failure;
							selector_set_interest_key(key, OP_WRITE);
						} else{
							ret = REQUEST_RESOLV;
							selector_set_interest_key(key, OP_NOOP);
						}
					}
					break;

				} default: {
					ret       = REQUEST_WRITE;
					d->status = status_address_type_not_supported;
					selector_set_interest_key(key, OP_WRITE);
				}
			}
			break;
		case socks_req_cmd_bind:
		case socks_req_cmd_associate:
		default:
			d->status = status_command_not_supported;
			ret = REQUEST_WRITE;
			break;
	}

	return ret;
}
*/
/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
/*static void *
resolv_connection_blocking(void *data) {
	struct selector_key *key = (struct selector_key *) data;
	struct socks5       *s   = ATTACHMENT(key);

	pthread_detach(pthread_self());
	s->origin_resolution = 0;
	struct addrinfo hints = {
		.ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 *
		.ai_socktype  = SOCK_STREAM,  /* Datagram socket *
		.ai_flags     = AI_PASSIVE,   /* For wildcard IP address *
		.ai_protocol  = 0,            /* Any protocol *
		.ai_canonname = NULL,
		.ai_addr      = NULL,
		.ai_next      = NULL,
	};

	char buff[7];
	snprintf(buff, sizeof(buff), "%d",
			 ntohs(s->client.request.request.dest_port));

	getaddrinfo(s->client.request.request.dest_addr.fqdn, buff, &hints,
			   &s->origin_resolution);

	selector_notify_block(key->s, key->fd);

	free(data);

	return 0;
}
*/

/** procesa el resultado de la resolución de nombres */
/*static unsigned
request_resolv_done(struct selector_key *key) {
	struct request_st * d = &ATTACHMENT(key)->client.request;
	struct socks5 *s      =  ATTACHMENT(key);

	if(s->origin_resolution == 0) {
		d->status = status_general_SOCKS_server_failure;
	} else {
		s->origin_domain   = s->origin_resolution->ai_family;
		s->origin_addr_len = s->origin_resolution->ai_addrlen;
		memcpy(&s->origin_addr,
				s->origin_resolution->ai_addr,
				s->origin_resolution->ai_addrlen);
		freeaddrinfo(s->origin_resolution);
		s->origin_resolution = 0;
	}

	return request_connect(key, d);
}*/

/** intenta establecer una conexión con el origin server */
/*static unsigned
request_connect(struct selector_key *key, struct request_st *d) {
	bool error                  = false;
	// da legibilidad
	enum socks_response_status status =  d->status;
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
			status = errno_to_socks(errno);
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

	d->status = status;

	return REQUEST_CONNECTING;
}

static void
request_read_close(const unsigned state, struct selector_key *key) {
	struct request_st * d = &ATTACHMENT(key)->client.request;

	request_close(&d->parser);
}*/

////////////////////////////////////////////////////////////////////////////////
// REQUEST CONNECT
////////////////////////////////////////////////////////////////////////////////
/*static void
request_connecting_init(const unsigned state, struct selector_key *key) {
	struct connecting *d  = &ATTACHMENT(key)->origin.conn;

	d->client_fd = &ATTACHMENT(key)->client_fd;
	d->origin_fd = &ATTACHMENT(key)->origin_fd;
	d->status    = &ATTACHMENT(key)->client.request.status;
	d->write_buffer        = &ATTACHMENT(key)->write_buffer;
}*/

/** la conexión ha sido establecida (o falló)  */
/*static unsigned
request_connecting(struct selector_key *key) {
	int error;
	socklen_t len = sizeof(error);
	struct connecting *d  = &ATTACHMENT(key)->origin.conn;

	if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		*d->status = status_general_SOCKS_server_failure;
	} else {
		if(error == 0) {
			*d->status     = status_succeeded;
			*d->origin_fd = key->fd;
		} else {
			*d->status = errno_to_socks(error);
		}
	}

	if(-1 == request_marshall(d->write_buffer, *d->status)) {
		*d->status = status_general_SOCKS_server_failure;
		abort(); // el buffer tiene que ser mas grande en la variable
	}
	selector_status s = 0;
	s |= selector_set_interest    (key->s, *d->client_fd, OP_WRITE);
	s |= selector_set_interest_key(key,                   OP_NOOP);
	return SELECTOR_SUCCESS == s ? REQUEST_WRITE : ERROR;
}

void
log_request(enum socks_response_status status,
			const struct sockaddr* clientaddr,
			const struct sockaddr* originaddr);*/

/** escribe todos los bytes de la respuesta al mensaje `request' */
/*static unsigned
request_write(struct selector_key *key) {
	struct request_st * d = &ATTACHMENT(key)->client.request;

	unsigned  ret       = REQUEST_WRITE;
	  buffer *b         = d->write_buffer;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_read_ptr(b, &count);
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		buffer_read_adv(b, n);

		if(!buffer_can_read(b)) {
			if(d->status == status_succeeded) {
				ret = COPY;
				selector_set_interest    (key->s,  *d->client_fd, OP_READ);
				selector_set_interest    (key->s,  *d->origin_fd, OP_READ);
			} else {
				ret = DONE;
				selector_set_interest    (key->s,  *d->client_fd, OP_NOOP);
				if(-1 != *d->origin_fd) {
					selector_set_interest    (key->s,  *d->origin_fd, OP_NOOP);
				}
			}
		}
	}

	log_request(d->status, (const struct sockaddr *)&ATTACHMENT(key)->client_addr,
						   (const struct sockaddr *)&ATTACHMENT(key)->origin_addr);
	return ret;
}*/

////////////////////////////////////////////////////////////////////////////////
// COPY
////////////////////////////////////////////////////////////////////////////////

/*static void
copy_init(const unsigned state, struct selector_key *key) {
	struct copy * d = &ATTACHMENT(key)->client.copy;

	d->fd        = &ATTACHMENT(key)->client_fd;
	d->read_buffer        = &ATTACHMENT(key)->read_buffer;
	d->write_buffer        = &ATTACHMENT(key)->write_buffer;
	d->duplex    = OP_READ | OP_WRITE;
	d->other     = &ATTACHMENT(key)->origin.copy;

	d = &ATTACHMENT(key)->origin.copy;
	d->fd       = &ATTACHMENT(key)->origin_fd;
	d->read_buffer       = &ATTACHMENT(key)->write_buffer;
	d->write_buffer       = &ATTACHMENT(key)->read_buffer;
	d->duplex   = OP_READ | OP_WRITE;
	d->other    = &ATTACHMENT(key)->client.copy;
}*/

/**
 * Computa los intereses en base a la disponiblidad de los buffer.
 * La variable duplex nos permite saber si alguna vía ya fue cerrada.
 * Arrancá OP_READ | OP_WRITE.
 */
/*static fd_interest
copy_compute_interests(fd_selector s, struct copy* d) {
	fd_interest ret = OP_NOOP;
	if ((d->duplex & OP_READ)  && buffer_can_write(d->read_buffer)) {
		ret |= OP_READ;
	}
	if ((d->duplex & OP_WRITE) && buffer_can_read (d->write_buffer)) {
		ret |= OP_WRITE;
	}
	if(SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret)) {
		abort();
	}
	return ret;
}*/

/** elige la estructura de copia correcta de cada fd (origin o client) */
/*static struct copy *
copy_ptr(struct selector_key *key) {
	struct copy * d = &ATTACHMENT(key)->client.copy;

	if(*d->fd == key->fd) {
		// ok
	} else {
		d = d->other;
	}
	return  d;
}*/

/** lee bytes de un socket y los encola para ser escritos en otro socket */
/*static unsigned
copy_r(struct selector_key *key) {
	struct copy * d = copy_ptr(key);

	assert(*d->fd == key->fd);

	size_t size;
	ssize_t n;
	buffer* b    = d->read_buffer;
	unsigned ret = COPY;

	uint8_t *ptr = buffer_write_ptr(b, &size);
	n = recv(key->fd, ptr, size, 0);
	if(n <= 0) {
		shutdown(*d->fd, SHUT_RD);
		d->duplex &= ~OP_READ;
		if(*d->other->fd != -1) {
			shutdown(*d->other->fd, SHUT_WR);
			d->other->duplex &= ~OP_WRITE;
		}
	} else {
		buffer_write_adv(b, n);
	}
	copy_compute_interests(key->s, d);
	copy_compute_interests(key->s, d->other);
	if(d->duplex == OP_NOOP) {
		ret = DONE;
	}
	return ret;
}*/

/** escribe bytes encolados */
/*static unsigned
copy_w(struct selector_key *key) {
	struct copy * d = copy_ptr(key);

	assert(*d->fd == key->fd);
	size_t size;
	ssize_t n;
	buffer* b = d->write_buffer;
	unsigned ret = COPY;

	uint8_t *ptr = buffer_read_ptr(b, &size);
	n = send(key->fd, ptr, size, MSG_NOSIGNAL);
	if(n == -1) {
		shutdown(*d->fd, SHUT_WR);
		d->duplex &= ~OP_WRITE;
		if(*d->other->fd != -1) {
			shutdown(*d->other->fd, SHUT_RD);
			d->other->duplex &= ~OP_READ;
		}
	} else {
		buffer_read_adv(b, n);
	}
	copy_compute_interests(key->s, d);
	copy_compute_interests(key->s, d->other);
	if(d->duplex == OP_NOOP) {
		ret = DONE;
	}
	return ret;
}
*/

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
