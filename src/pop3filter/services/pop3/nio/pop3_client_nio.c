/**
 * socks5nio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
 */

#include "include/pop3_client_nio.h"

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
	DOT_DATA_SREAD,
	DOT_DATA_CWRITE,

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
	PIPE_DREAM,

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
	buffer 							*read_buffer, *write_buffer, *aux_buffer;
	struct pop3_command_builder     *current_command;
	size_t 							*current_command_len;

	/** parser */
	struct parser 					*command_parser;
	bool 							*pipelined;

	const int 						*client_fd;
	const int 						*origin_fd;
	size_t 							*bytes_in_buffer;
	size_t 							*bytes_read;
};

/** usado por REQUEST_CONNECTING */
struct response_st {
	buffer     						*write_buffer, *read_buffer, *trans_buffer;
	const int  						*client_fd;
	const int        				*origin_fd;
	bool 							*pipelined;
	struct parser 					*singleline_parser;
	struct parser 					*multiline_parser;
	struct pop3_command_builder     *current_command;
	bool 							*transform;

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
	bool 							has_pipelining;
	bool 							pipelined;
	size_t 							bytes_in_buffer;
	size_t 							current_command_len;
	size_t 							bytes_read;


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

	bool 							 transform;

	/** buffers para ser usados read_bdescribe_statesuffer, write_buffer.*/
	//pop3_new
	uint8_t raw_buff_a[2048], raw_buff_b[2048], raw_buff_trans[3000];

	//pop3_new
	buffer read_buffer, write_buffer, aux_buffer, trans_buffer;
	
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
static struct pop3 * pool      	 = 0;  // pool propiamente dicho

// curr_origin_server = {
// 	.dest_addr_type = req_addrtype_domain, 
// 	.dest_addr = {
// 		.fqdn = "pop.fibertel.com.ar"
// 	},
// 	.dest_port = 28160,
// };

static const struct state_definition *
describe_states(void);

void
init_curr_origin_server(int address_type,char addr[0xff], in_port_t dest_port,struct sockaddr_in ipv4,struct sockaddr_in6 ipv6 ){
	curr_origin_server = malloc(sizeof(*curr_origin_server));
	if(address_type == req_addrtype_domain){
		//Resolver nombre
		//curr_origin_server->dest_addr.fqdn = addr;
		strncpy(curr_origin_server->dest_addr.fqdn, addr, sizeof(addr));
		curr_origin_server->dest_addr_type = req_addrtype_domain;
		curr_origin_server->dest_port 		= dest_port;
	}
	else if(address_type == req_addrtype_ipv4)
	{
		curr_origin_server->dest_addr.ipv4 = ipv4;
		curr_origin_server->dest_addr_type = req_addrtype_ipv4;
		curr_origin_server->dest_port = dest_port;
	}
	else{
		curr_origin_server->dest_addr.ipv6 = ipv6;
		curr_origin_server->dest_addr_type = req_addrtype_ipv6;
		curr_origin_server->dest_port = dest_port;
	}
}

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
	ret->origin_server.dest_addr_type 	= curr_origin_server->dest_addr_type;
	ret->origin_server.dest_addr 		= curr_origin_server->dest_addr;
	ret->origin_server.dest_port 		= curr_origin_server->dest_port;

	ret->stm    .initial   = PROCESS_CONNECTION;
	ret->stm    .max_state = ERROR;
	ret->stm    .states    = describe_states();
	stm_init(&ret->stm);

	buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
	buffer_init(&ret->aux_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
	buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

	ret->references = 1;
	fprintf(stderr,"ETREEEE\n");
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
	fprintf(stderr,"Entre a passive accept\n");
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
		//fprintf(stderr,"ETREEEE\n");
	fprintf(stderr,"TERMINO ESTADO");
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
	d->client_fd       = &ATTACHMENT(key)->client_fd;
	d->origin_fd       = &ATTACHMENT(key)->origin_fd;
	d->origin_server.dest_addr_type 	= curr_origin_server->dest_addr_type;
	d->origin_server.dest_addr 		= curr_origin_server->dest_addr;
	d->origin_server.dest_port 		= curr_origin_server->dest_port;
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
			fprintf(stderr,"Valor 1: %d\n",d->origin_server.dest_addr.ipv4.sin_port);
			ATTACHMENT(key)->origin_addr_len =
					sizeof(d->origin_server.dest_addr.ipv4);
			memcpy(&ATTACHMENT(key)->origin_addr, &d->origin_server.dest_addr,
					sizeof(d->origin_server.dest_addr.ipv4));
			fprintf(stderr,"Valor 2: %d\n",d->origin_server.dest_addr.ipv4.sin_addr.s_addr);
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

void breakpoint(){

}
static unsigned
greeting_sread(struct selector_key *key) {
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
	if(n > 0) {
		printf("POINTER GREETING %s\n", ptr);
		buffer_write_adv(d->read_buffer, n);
		state = pop3_singleline_response_builder(ptr, n, d->singleline_parser, &builder, &error);
		//struct parser_event* event = pop3_singleline_parser_consume(d->read_buffer, d->singleline_parser, &error);
		selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
		if(state == BUILD_FINISHED && SELECTOR_SUCCESS == selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
			ret = GREETING_CWRITE;
		} else {
			ret = ERROR;
		}
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
	return ret;
}

static void
command_init(const unsigned state, struct selector_key *key) {
	struct command_st * d = &ATTACHMENT(key)->client.command;
	printf("command_init\n");
	d->read_buffer              = &(ATTACHMENT(key)->read_buffer);
	d->write_buffer             = &(ATTACHMENT(key)->write_buffer);
	d->aux_buffer             = &(ATTACHMENT(key)->aux_buffer);
	buffer_init(d->write_buffer, N(ATTACHMENT(key)->raw_buff_b), ATTACHMENT(key)->raw_buff_b);
	d->command_parser 			= pop3_command_parser_init();
	d->pipelined 				= &(ATTACHMENT(key)->pipelined);
	d->current_command 			= &(ATTACHMENT(key)->current_command);
	d->current_command_len 			= &(ATTACHMENT(key)->current_command_len);
	d->bytes_read 			= &(ATTACHMENT(key)->bytes_read);
	*d->current_command_len 			= 0;
	*d->bytes_read 			= 0;
	d->current_command->has_args = false;
	d->current_command->kwrd_ptr = 0;
	d->bytes_in_buffer 			= &(ATTACHMENT(key)->bytes_in_buffer);
	memset(d->current_command->kwrd, '\0', MAX_KEYWORD_LENGTH);
	buffer_reset(d->read_buffer);
	buffer_reset(d->write_buffer);
	buffer_reset(d->aux_buffer);
	printf("At end\n");
}

static unsigned
command_cread(struct selector_key *key) {
	struct command_st *d = &ATTACHMENT(key)->client.command;
	unsigned  ret      = COMMAND_CREAD;
		bool  error    = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	 enum structure_builder_states 	 			state;
	 size_t  command_length;
	 printf("Llegue\n");
	ptr = buffer_write_ptr(d->read_buffer, &count);
	n = recv(key->fd, ptr, count, 0);
	if(n > 0) {
		buffer_write_adv(d->read_buffer, n);
		state = command_builder(ptr, n, &command_length, d->command_parser, d->current_command, &error);
		d->read_buffer->write = ptr;
		buffer_write_adv(d->read_buffer, command_length);
		*d->current_command_len += command_length;
		*d->bytes_read 			+= n;
		//d->aux_buffer->write = ptr;
		//buffer_write_adv(d->aux_buffer, command_length);
		//uint8_t * ptr2 = buffer_read_ptr(d->aux_buffer, &count);
		//d->aux_buffer->write = d->read_buffer->write;
		//printf("PTR 2 %d %*s\n", command_length, count, ptr2);
		if(state == BUILD_FINISHED ){
			//buffer_reset(d->aux_buffer);
			if(n != command_length){
				printf("Im going to be pipelined %c\n", *d->read_buffer->write);
				*(d->pipelined) = true;
				*(d->bytes_in_buffer) = n-command_length;
			} else {
				printf("Im NOT going to be pipelined %d %d\n", command_length, n);
				*(d->pipelined) = false;
			}
			printf("current command %s\n", d->current_command->kwrd);
			printf("BUILD_FINISHED %s", ptr);
			selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
			if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE)){
				ret = COMMAND_SWRITE;
			} else {
				ret = ERROR;
			}
			
		} else if(state == BUILDING){
			printf("State building\n");
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				ret = COMMAND_CREAD;
			} else {
				ret = ERROR;
			}
		} else {
			printf("state else building\n");
			ret = ERROR;
		}
	} else {
		ret = ERROR;
	}
	return error ? ERROR : ret;
}
static void
pipe_init(const unsigned state, struct selector_key *key) {
	struct command_st * d = &ATTACHMENT(key)->client.command;

	d->read_buffer              = &(ATTACHMENT(key)->read_buffer);
	d->write_buffer             = &(ATTACHMENT(key)->write_buffer);
	//d->aux_buffer             = &(ATTACHMENT(key)->aux_buffer);
	//buffer_init(d->write_buffer, N(ATTACHMENT(key)->raw_buff_b), ATTACHMENT(key)->raw_buff_b);
	d->command_parser 			= pop3_command_parser_init();
	d->pipelined 				= &(ATTACHMENT(key)->pipelined);
	d->current_command 			= &(ATTACHMENT(key)->current_command);
	d->bytes_in_buffer 			=&(ATTACHMENT(key)->bytes_in_buffer);
	d->current_command->has_args = false;
	d->current_command->kwrd_ptr = 0;
	memset(d->current_command->kwrd, '\0', MAX_KEYWORD_LENGTH);
	//buffer_reset(d->read_buffer);
	//buffer_reset(d->write_buffer);
	//buffer_reset(d->aux_buffer);
}

static unsigned
pipe_dream(struct selector_key *key) {
	struct command_st *d = &ATTACHMENT(key)->client.command;
	unsigned  ret      = PIPE_DREAM;
		bool  error    = false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	 enum structure_builder_states 	 			state;
	 size_t  command_length;
	 printf("PIPE_DREAM\n");
	printf("Im being pipelined %c\n", *d->read_buffer->write);
	ptr = buffer_write_ptr(d->read_buffer, &count);
	printf("buffer_write %d %s\n", count, ptr);
	if(*d->bytes_in_buffer > 0) {
		buffer_write_adv(d->read_buffer, *d->bytes_in_buffer);
		state = command_builder(ptr, *d->bytes_in_buffer, &command_length, d->command_parser, d->current_command, &error);
		d->read_buffer->write = ptr;
		buffer_write_adv(d->read_buffer, command_length);
		*d->bytes_in_buffer -= command_length;
		printf("buffer_write 2 %d %s\n", command_length, ptr);
		//d->aux_buffer->write = ptr;
		//buffer_write_adv(d->aux_buffer, command_length);
		//uint8_t * ptr2 = buffer_read_ptr(d->aux_buffer, &count);
		//d->aux_buffer->write = d->read_buffer->write;
		//printf("PTR 2 %d %*s\n", command_length, count, ptr2);
		//abort();
		if(state == BUILD_FINISHED ){
			//buffer_reset(d->aux_buffer);
			printf("%s", d->current_command->kwrd);
			selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
			if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE)){
				ret = COMMAND_SWRITE;
			} else {
				ret = ERROR;
			}
			
		} else if(state == BUILDING){
			printf("State building\n");
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
				ret = PIPE_DREAM;
			} else {
				ret = ERROR;
			}
		} else {
			printf("state else building\n");
			ret = ERROR;
		}
	} else {
		selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
		if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ)){
			//buffer_reset(d->read_buffer);
			printf("LISTO CALISTO\n");
			ret = COMMAND_CREAD;
		} else {
			ret = ERROR;
		}
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

	ptr = buffer_read_ptr(d->read_buffer, &count);
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	printf("Este es el cmd %d %d %*s\n", count, n, n, ptr);
	if(n == -1) {
		ret = ERROR;
	} else {
		d->read_buffer->read += n;
		printf("Im command %c\n", *d->read_buffer->write);
		if(!buffer_can_read(d->read_buffer)) {
		
			//key->fd = ATTACHMENT(key)->origin_fd;
			if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
				printf("Selector SUCCESS!\n");
				ret = RESPONSE_SREAD;
			} else {
				printf("Print command_swrite error!\n");
				ret = ERROR;
			}
		}
	}

	return ret;
}

/** inicializa las variables de los estados HELLO_… */
static void
response_init(const unsigned state, struct selector_key *key) {

	struct response_st *d 		= &ATTACHMENT(key)->origin.response;
	d->read_buffer              = &(ATTACHMENT(key)->read_buffer);
	d->write_buffer             = &(ATTACHMENT(key)->write_buffer);
	d->trans_buffer             = &(ATTACHMENT(key)->trans_buffer);
	d->current_command 			= &(ATTACHMENT(key)->current_command);
	d->singleline_parser 		= pop3_singleline_response_parser_init();
	d->multiline_parser 		= pop3_multiline_response_parser_init();
	d->transform 			= &ATTACHMENT(key)->transform;
	d->pipelined 				= &(ATTACHMENT(key)->pipelined);
	buffer_init(d->write_buffer,N(ATTACHMENT(key)->raw_buff_b),ATTACHMENT(key)->raw_buff_b);
	buffer_init(d->trans_buffer,N(ATTACHMENT(key)->raw_buff_trans),ATTACHMENT(key)->raw_buff_trans);
	//buffer_init(d->read_buffer,N(ATTACHMENT(key)->raw_buff_a),ATTACHMENT(key)->raw_buff_a);	
	//buffer_reset(d->read_buffer);
	buffer_reset(d->write_buffer);
	buffer_reset(d->trans_buffer);
}


static unsigned
response_sread(struct selector_key *key) {
	struct response_st *d 	= &ATTACHMENT(key)->origin.response;
	unsigned  ret      		= RESPONSE_SREAD;
		bool  error    		= false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	  enum structure_builder_states 	 			state;
	 struct pop3_singleline_response_builder 		builder;
	ptr = buffer_write_ptr(d->write_buffer, &count);
	n = recv(key->fd, ptr, count, 0);
	printf("Im response %c\n", *d->read_buffer->write);
	if(n > 0) {
		buffer_write_adv(d->write_buffer, n);
		state = pop3_singleline_response_builder(ptr, n, d->singleline_parser, &builder, &error);
		if(state == BUILD_FINISHED){
			enum pop3_command_types type = get_command_type(d->current_command->kwrd);
			switch(type){
				case OVERLOADED:
					printf("OVERLOADED\n");
					if(d->current_command->has_args){
						break;
					}
				case MULTILINE:
					printf("MULTILINE\n");
					enum consumer_state c_state = pop3_multiline_response_checker(ptr, n, d->multiline_parser, &error);
					printf("En mlt\n");
					if(strcmp(d->current_command->kwrd, "RETR") == 0 || strcmp(d->current_command->kwrd, "TOP") == 0){
						printf("Debo transform\n");
						//buffer_reset(d->trans_buffer);
						*(d->transform) = true;
					}else {
						printf("NO Debo transform\n");
						*(d->transform) = false;
						printf("Transformed == false\n");
					}
					printf("A punto de transform\n");
					if(*(d->transform)){
						int bytes_to_read;
						uint8_t *ptr = buffer_read_ptr((d->write_buffer), (&bytes_to_read));
						printf("Antes de leer %d\n",bytes_to_read);
						int resp = create_transformation(ATTACHMENT(key)->sender_pipe, ATTACHMENT(key)->receiver_pipe);
						write(ATTACHMENT(key)->sender_pipe[1], ptr, bytes_to_read);
						//						buffer_reset(d->trans_buffer);

						uint8_t *read_ptr = buffer_write_ptr(d->trans_buffer, &bytes_to_read);
						printf("Antes de leer\n");
						printf("aca hay algo distintivoo0000!!! %s %d\n",read_ptr,bytes_to_read);
						size_t u =read(ATTACHMENT(key)->receiver_pipe[0], read_ptr, bytes_to_read);

						printf("aca hay algo distintivoo %s %d con u %d\n",read_ptr,bytes_to_read,u);
						printf("DESPues de leer\n");
						buffer_write_adv(d->trans_buffer, u);
						if(c_state != FINISHED_CONSUMING ){
							selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
							if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
								printf("SE VA A CWRITE\n");
							return DOT_DATA_CWRITE;
							}
						} else {
							selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
							if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
								ret = RESPONSE_CWRITE;
							} else {
								ret = ERROR;
							}
							return ret;
						}
					}
					printf("Vamos a ver cual es el problema");
					fflush(stdout);
					if(c_state == FINISHED_CONSUMING ){
						printf("Quiero RESP_CWRITE");
						fflush(stdout);
						selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
						if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
							ret = RESPONSE_CWRITE;
						} else {
							ret = ERROR;
						}
						return ret;
					} else {
						printf("Quiero DOT_DATA_CWRITE\n");
						fflush(stdout);
						selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
						if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
							printf("I'm about to be killed\n");
							printf("%s\n", ptr);
							fflush(stdout);
							breakpoint();
							ret = DOT_DATA_CWRITE;
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

static void
dot_data_init(const unsigned state, struct selector_key *key) {
	printf("WE init\n");
	struct response_st *d 		= &ATTACHMENT(key)->origin.response;
	d->transform 				= &ATTACHMENT(key)->transform;
	buffer_reset(d->write_buffer);
	buffer_reset(d->trans_buffer);
}

static unsigned
dot_data_sread(struct selector_key *key){
	printf("READY TO READ\n");
	struct response_st *d 	= &ATTACHMENT(key)->origin.response;
	unsigned  ret      		= DOT_DATA_SREAD;
		bool  error    		= false;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	  enum consumer_state 	 consumer_state;
	  
	ptr = buffer_write_ptr(d->write_buffer, &count);
	n = recv(key->fd, ptr, count, 0);
	printf("IMMMM HERE %d %s\n", n, ptr);
	if(n > 0) {
		int bytes_to_read;
		buffer_write_adv(d->write_buffer, n);
		if(*(d->transform)){
				uint8_t *ptr = buffer_read_ptr((d->write_buffer), (&bytes_to_read));
				printf("Antes de leer DOT DATA CWRITE %d\n", bytes_to_read);
				//int resp = create_transformation(ATTACHMENT(key)->sender_pipe, ATTACHMENT(key)->receiver_pipe);
				write(ATTACHMENT(key)->sender_pipe[1], ptr, bytes_to_read);
				//buffer_reset(d->read_buffer);
				buffer_reset(d->trans_buffer);

				uint8_t *read_ptr = buffer_write_ptr(d->trans_buffer, &bytes_to_read);
				printf("Antes de leer DOT DATA CWRITE\n");
				printf("aca hay algo distintivoo SREAD %s %d\n", read_ptr, bytes_to_read);
				size_t u= read(ATTACHMENT(key)->receiver_pipe[0], read_ptr, bytes_to_read);
				buffer_write_adv(d->trans_buffer,u);
				printf("aca hay algo distintivoo SREAD%s %d %u\n",read_ptr,bytes_to_read,u);
				printf("DESPues de leer\n");
		}
				

				
		// consumer_state = pop3_multiline_response_checker(ptr, n, d->multiline_parser, &error);
		// printf("SHAzAM\n %d\n", *(d->transform));
		// int bytes_to_read;

		// uint8_t *ptr = buffer_read_ptr(d->write_buffer, &bytes_to_read);

		// write(ATTACHMENT(key)->sender_pipe[1], ptr, bytes_to_read);
		// uint8_t *read_ptr = buffer_write_ptr(d->read_buffer, &bytes_to_read);
		// read(ATTACHMENT(key)->receiver_pipe[0], read_ptr, bytes_to_read);
		/*if(*(d->transform)){
			int bytes_to_read;
			uint8_t *ptr = buffer_read_ptr(d->write_buffer, &bytes_to_read);
			int resp = create_transformation(ATTACHMENT(key)->sender_pipe, ATTACHMENT(key)->receiver_pipe);
			write(ATTACHMENT(key)->sender_pipe[1], ptr, bytes_to_read);
			uint8_t *read_ptr = buffer_write_ptr(d->read_buffer, &bytes_to_read);
			read(ATTACHMENT(key)->receiver_pipe[0], read_ptr, bytes_to_read);
		}*/
		if(consumer_state != FINISHED_CONSUMING ){
			printf("NOT FINISHED_CONSUMING\n");
			selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
			if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
				ret = DOT_DATA_CWRITE;
			} else {
				ret = ERROR;
			}
		} else {
			printf("FINISHED_CONSUMING\n");
			selector_set_interest    (key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);
			if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)){
				ret = RESPONSE_CWRITE;
			} else {
				ret = ERROR;
			}
			return ret;
		}
	} else {
		ret = ERROR;
	}

	return error ? ERROR : ret;
}

static unsigned
dot_data_cwrite(struct selector_key *key) {
	printf("dot_data_cwrite\n");
	fflush(stdout);
	struct response_st *d = &ATTACHMENT(key)->origin.response;
	  enum structure_builder_states 	 			state;
		bool  error    		= false;
	 struct pop3_singleline_response_builder 		builder;

	unsigned  ret     = DOT_DATA_CWRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	if(*(d->transform)){
		ptr = buffer_read_ptr(d->trans_buffer, &count);
	} else {
		ptr = buffer_read_ptr(d->write_buffer, &count);
	}
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		if(*(d->transform)){
			buffer_read_adv(d->trans_buffer, n);
			state = pop3_singleline_response_builder(ptr, n, d->singleline_parser, &builder, &error);
			if(!buffer_can_read(d->trans_buffer)) {
				selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
				if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ)){
					ret = DOT_DATA_SREAD;
				} else {
					ret = ERROR;
				}
			}
		} else {
			buffer_read_adv(d->write_buffer, n);
			state = pop3_singleline_response_builder(ptr, n, d->singleline_parser, &builder, &error);
			if(!buffer_can_read(d->write_buffer)) {
				selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
				if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ)){
					ret = DOT_DATA_SREAD;
				} else {
					ret = ERROR;
				}
			}
		}
		
		
	}

	return ret;
}

static unsigned
response_cwrite(struct selector_key *key) {
	/*
	printf("response_cwrite\n");
	struct response_st *d = &ATTACHMENT(key)->origin.response;

	unsigned  ret     = RESPONSE_CWRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;

	ptr = buffer_read_ptr(d->write_buffer, &count);
	printf("PTR %s %d %p %p\n", ptr, count, d->write_buffer->read, d->write_buffer->write);
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		buffer_read_adv(d->write_buffer, n);
		if(!buffer_can_read(d->write_buffer)) {
			if(*(d->pipelined)){
				selector_set_interest    (key->s, ATTACHMENT(key)->client_fd, OP_NOOP);
				if(SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE)){
					ret = PIPE_DREAM;
				} else {
					ret = ERROR;
				}
			} else {
				if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
					ret = COMMAND_CREAD;
				} else {
					ret = ERROR;
				}
			}
		}
	}*/
	struct response_st *d = &ATTACHMENT(key)->origin.response;
	  enum structure_builder_states 	 			state;
		bool  error    		= false;
	 struct pop3_singleline_response_builder 		builder;

	unsigned  ret     = RESPONSE_CWRITE;
	 uint8_t *ptr;
	  size_t  count;
	 ssize_t  n;
	 	

	if(*(d->transform)){
		ptr = buffer_read_ptr(d->trans_buffer, &count);
		fprintf(stderr,"buffer trans en cwrite %d %s\n", count, d->trans_buffer->read);
	} else {
		ptr = buffer_read_ptr(d->write_buffer, &count);
	}
	n = send(key->fd, ptr, count, MSG_NOSIGNAL);
	if(n == -1) {
		ret = ERROR;
	} else {
		if(*(d->transform)){
			buffer_read_adv(d->trans_buffer, n);
			state = pop3_singleline_response_builder(ptr, n, d->singleline_parser, &builder, &error);
			if(!buffer_can_read(d->trans_buffer)) {
				if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
					fprintf(stderr,"ENTRO PIPE DREAM\n");
					ret = COMMAND_CREAD;
				} else {
					ret = ERROR;
				}
			}
		} else {
			buffer_read_adv(d->write_buffer, n);
			state = pop3_singleline_response_builder(ptr, n, d->singleline_parser, &builder, &error);
			if(!buffer_can_read(d->write_buffer)) {
				if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
					ret = COMMAND_CREAD;
				} else {
					ret = ERROR;
				}
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
		.state 				= DOT_DATA_SREAD,
		.on_arrival 		= dot_data_init,
		.on_read_ready 		= dot_data_sread,
	},{
		.state 				= DOT_DATA_CWRITE,
		.on_write_ready		= dot_data_cwrite,
	},{
		.state				= RESPONSE_CWRITE,
		.on_write_ready		= response_cwrite,
	},{
		.state 				= PIPE_DREAM,
		.on_arrival 		= pipe_init,
		.on_write_ready 	= pipe_dream,
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
