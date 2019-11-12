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
#include <netinet/sctp.h>

#include "../../include/buffer.h"

#include "../../include/stm.h"
#include "include/pop3_admin_nio.h"
#include "../../../utils/include/netutils.h"
#include "../../speedwagon/include/speedwagon_decoder.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

static unsigned
request_read(struct selector_key *key);

static unsigned
response_write(struct selector_key *key);

enum pop3_admin_states {
	REQUEST_READ,
	ERROR,
};

struct pop3_admin {
	/** Administrator Information */
	struct sockaddr_storage			admin_addr;
	socklen_t						admin_addr_len;

	int 							admin_fd;

	bool 							authenticated;

	uint8_t							buffer[MAX_BUFFER];

	/** State Machines*/
	struct state_machine			stm;

	/** Amount of reference to this object, if one it should be destroyed */
	unsigned references;

	/** Next in pool */
	struct pop3_admin *next;
};

/** Handler definition for each state */
static const struct state_definition client_statbl[] = {
	{
		.state 					= 	REQUEST_READ,
		.on_read_ready 			= 	request_read,
	}, {
		.state 					= 	ERROR,
	}
};

static const struct state_definition *
describe_states(void) {
	return client_statbl;
}


/**
 * Pool de `struct socks5', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */

static const unsigned		max_pool  = 50; // tamaño máximo
static unsigned				pool_size = 0;  // tamaño actual
static struct pop3_admin*	pool      = 0;  // pool propiamente dicho

static const struct state_definition *
describe_states(void);

/** crea un nuevo `struct socks5' */
static struct pop3_admin *
pop3_admin_new(int admin_fd) {
	struct pop3_admin *ret;

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

	ret->admin_fd       = admin_fd;
	ret->admin_addr_len = sizeof(ret->admin_addr);

	ret->authenticated 	= false;

	ret->stm    .initial   = REQUEST_READ;
	ret->stm    .max_state = ERROR;
	ret->stm    .states    = describe_states();
	stm_init(&ret->stm);

	ret->references = 1;
finally:
	return ret;
}



/**
 * destruye un  `struct socks5', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
destroy_suscription(struct pop3_admin *s) {
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
	struct pop3_admin *next, *s;
	for(s = pool; s != NULL ; s = next) {
		next = s->next;
		free(s);
	}
}

/** obtiene el struct (socks5 *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3_admin *)(key)->data)

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
pop3_admin_passive_accept(struct selector_key *key) {
	struct sockaddr_storage       admin_addr;
	socklen_t                     admin_addr_len = sizeof(admin_addr);
	struct pop3_admin          	  *state           = NULL;
	const int admin = accept(key->fd, (struct sockaddr*) &admin_addr,
														  &admin_addr_len);
	if(admin == -1) {
		goto fail;
	}
	if(selector_fd_set_nio(admin) == -1) {
		goto fail;
	}

	state = pop3_admin_new(admin);
	if(state == NULL) {
		// sin un estado, nos es imposible manejaro.
		// tal vez deberiamos apagar accept() hasta que detectemos
		// que se liberó alguna conexión.
		goto fail;
	}
	memcpy(&state->admin_addr, &admin_addr, admin_addr_len);
	state->admin_addr_len = admin_addr_len;

	if(SELECTOR_SUCCESS != selector_register(key->s, admin, &suscriptions_handler,
											  OP_READ, state)) {
		goto fail;
	}
	return ;
fail:
	if(admin != -1) {
		close(admin);
	}
	destroy_suscription(state);
}

static unsigned
request_read(struct selector_key *key) {
	unsigned  ret			= REQUEST_READ;
	 uint8_t *buffer 		= ATTACHMENT(key)->buffer;
		bool  authenticated = ATTACHMENT(key)->authenticated;
	  size_t  count 		= MAX_BUFFER;
	 ssize_t  n;
	 char **  parameters;
	    char  response;
	     int  pointer=2;
			 int  bytes_transfered=0;
    bzero(buffer, sizeof(buffer));
	n = recv(key->fd, &ATTACHMENT(key)->buffer, count, 0);
	if(n > 0) {

		if(authenticated){
			// AUTHENTICATED
		} else {
			// NOT AUTHENTICATED
		}
		buffer[n] = 0;
		//if (sndrcvinfo.sinfo_stream == LOCALTIME_STREAM) {
		printf("PROTOCOL RESPONSE: THE eBYTE is ");
		fflush(stdout);
		show_byte(buffer[0]);
		int size = buffer[1];
		if(size>0){
			printf("Quantity of parameters are: %d\n", size);
			if((parameters=malloc(size*sizeof(char*)))==NULL){
				printf("Error. Cannot allocate memory for parameters[].\n");
				return 1;
			}
			for (int i=0; i<size; i++){
				parameters[i]=malloc(strlen(buffer+pointer)+1);
				//printf("Reservando memoria para el parametro %d.....\n", i+1);
				//printf("La cantidad de memoria reservada es: %d\n", strlen(buffer+pointer)+1);
				strcpy(parameters[i],buffer+pointer);
				//printf("Se acaba de copiar la palabra: %s\n",buffer+pointer );
				pointer+= strlen(buffer+pointer)+1;
				//printf("El nuevo valor del puntero es: %d\n",pointer );
				//printf("%c\n", i+1);
				bytes_transfered+=strlen(buffer+pointer)+1;
			}
			printf("Parameters are: ");
			for(int i=0; i<size; i++){
				printf("%s ", parameters[i]);
			}
			printf("\n");
		}
		bytes_transfered=2;
		pointer=2;
		/*for(int i=0; i < 10; i++){
			putchar(buffer[i]);
		}*/
		//printf("%s ", parameters[2]);
		response=decode_request(buffer[0], parameters, size);


		fflush(stdout);
		buffer[0]=response;
		ret = response_write(key);
	} else {
		ret = ERROR;
	}

	return ret;
}

static unsigned
response_write(struct selector_key *key) {
	unsigned  ret     	= REQUEST_READ;
	 uint8_t *buffer 	= ATTACHMENT(key)->buffer;
	 // COUNT MUST BE SET TO THE LENGTH OF WHAT TO WRITE
	  size_t  count 	= sizeof(char);
	 ssize_t  n;

	n = send(key->fd, buffer, count, MSG_NOSIGNAL);
	if(n == -1 || SELECTOR_SUCCESS != selector_set_interest_key(key, OP_READ)) {
		ret = ERROR;
	}

	return ret;
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
		ATTACHMENT(key)->admin_fd,
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
