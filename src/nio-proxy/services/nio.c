/**
 * socks5nio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
*/

struct nio_info {
	struct sockaddr_storage			 connection_addr;
	socklen_t						 connection_addr_len;
	int 							 connection_fd;
	
	/** State Machines */
	struct state_machine			 stm;
	
	/** Structure dependent on the State Machine */
	void 							*structure;
	
	/** Amount of reference to this object, if one it should be destroyed */
	unsigned 						 references;
	struct nio_info					*next;
}

#define ATTACHMENT(key) ( (struct nio_info *)(key)->data)

/**
 * Pool de `struct socks5', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */

static const unsigned		 max_pool  = 50; // tamaño máximo
static unsigned				 pool_size = 0;  // tamaño actual
static struct nio_info		*pool      = 0;  // pool propiamente dicho

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

static inline struct nio_info*
init_nio_info(void){
	if(pool == NULL) {
		ret = malloc(sizeof(*ret));
	} else {
		ret       = pool;
		pool      = pool->next;
		ret->next = 0;
	}
}

/** crea un nuevo `struct socks5' */
static struct nio_info *
nio_new(int connection_fd, struct state_machine stm, (void)(*init_structure)(void*)) {
	struct nio_info *ret;

	ret = init_nio_info();

	if(ret == NULL) {
		goto finally;
	}

	memset(ret, 0x00, sizeof(*ret));

	ret->connection_fd       = connection_fd;
	ret->connection_addr_len = sizeof(ret->connection_addr);

	init_structure(ret->structure);

	ret->stm 				= stm;
	stm_init(&ret->stm);

	ret->references = 1;
finally:
	return ret;
}

/** Attemps to accept an incomming connection */
void
passive_accept(struct selector_key *key) {
	struct sockaddr_storage        connection_addr;
	socklen_t                      connection_addr_len	= sizeof(connection_addr);
	struct nio_info          	  *state 				= NULL;
	const int connection_fd = accept(key->fd, (struct sockaddr*) &connection_addr,
														  &connection_addr_len);
	if(connection_fd == -1) {
		goto fail;
	}
	if(selector_fd_set_nio(connection_fd) == -1) {
		goto fail;
	}

	state = nio_new(connection_fd);
	if(state == NULL) {
		// sin un estado, nos es imposible manejaro.
		// tal vez deberiamos apagar accept() hasta que detectemos
		// que se liberó alguna conexión.
		goto fail;
	}
	memcpy(&state->connection_addr, &connection_addr, connection_addr_len);
	state->connection_addr_len = connection_addr_len;

	if(SELECTOR_SUCCESS != selector_register(key->s, connection_fd, &suscriptions_handler,
											  OP_READ, state)) {
		goto fail;
	}
	return ;
fail:
	if(connection_fd != -1) {
		close(connection_fd);
	}
	destroy_suscription(state);
}

///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.
static void
finished_suscription(struct selector_key* key);

static void
read_suscription(struct selector_key *key) {
	struct state_machine *stm   = &ATTACHMENT(key)->stm;
	const enum pop3_admin_states st = stm_handler_read(stm, key);

	if(ERROR == st) {
		finished_suscription(key);
	}
}

static void
write_suscription(struct selector_key *key) {
	struct state_machine *stm   = &ATTACHMENT(key)->stm;
	const enum pop3_admin_states st = stm_handler_write(stm, key);

	if(ERROR == st) {
		finished_suscription(key);
	}
}

static void
block_suscription(struct selector_key *key) {
	struct state_machine *stm   = &ATTACHMENT(key)->stm;
	const enum pop3_admin_states st = stm_handler_block(stm, key);

	if(ERROR == st) {
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
		ATTACHMENT(key)->connection_fd,
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


/**
 * destruye un  `struct socks5', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
destroy_suscription(struct nio_info *s) {
	if(s == NULL) {
		// nada para hacer
	} else if(s->references == 1) {
		if(s != NULL) {
			if(pool_size < max_pool) {
				s->next = pool;
				pool    = s;
				pool_size++;
			}
		}
	} else {
		s->references -= 1;
	}
}

void
destroy_suscription_pool(void) {
	struct nio_info *next, *s;
	for(s = pool; s != NULL ; s = next) {
		next = s->next;
		free(s);
	}
}