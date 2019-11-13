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

static const struct state_definition *
describe_states(void);

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
static const struct state_definition admin_statbl[] = {
	{
		.state 					= 	REQUEST_READ,
		.on_read_ready 			= 	request_read,
	}, {
		.state 					= 	ERROR,
	}
};

static const struct state_definition *
describe_states(void) {
	return admin_statbl;
}

/** obtiene el struct (socks5 *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3_admin *)(key)->data)

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
		show_byte(buffer[0]);
		int size = buffer[1];
		if((parameters=malloc(size*sizeof(char*)))==NULL){
			printf("Error. No se pudo crear memoria para parameters[].\n");
			return 1;
		}
		for (int i=0; i<size; i++){
			parameters[i]=malloc(sizeof(buffer+pointer));
			strcpy(parameters[i],buffer+pointer);
			pointer+= sizeof(buffer+pointer);
			//printf("%c\n", i+1);
		}
		/*for(int i=0; i < 10; i++){
			putchar(buffer[i]);
		}*/
		printf("Los parametros son: ");
		for(int i=0; i<size; i++){
		  printf("%s ", parameters[i]);
		}
		printf("\n");
		//printf("%s ", parameters[2]);
		response=decode_request(buffer[0], parameters);
		
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