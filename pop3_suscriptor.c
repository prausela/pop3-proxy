
#include <stdlib.h>
#include <signal.h>

#include "pop3_admin_nio.h"
#include "pop3_suscriptor.h"

bool done = false;
 
selector_status 
init_suscription_service(const int server, const char **err_msg){
	selector_status   ss      = SELECTOR_SUCCESS;
	fd_selector selector      = NULL;

	const struct selector_init conf = {
		.signal = SIGALRM,
		.select_timeout = {
			.tv_sec  = 10,
			.tv_nsec = 0,
		},
	};

	if(0 != selector_init(&conf)) {
		*err_msg = "initializing selector";
		goto term_suscription_service;
	}

	selector = selector_new(1024);
	if(selector == NULL) {
		*err_msg = "unable to create selector";
		goto term_suscription_service;
	}

	//Suscription for accepts
	/*const struct fd_handler pop3 = {
		.handle_read       = pop3_passive_accept,
		.handle_write      = NULL,
		.handle_close      = NULL, // nada que liberar
	};*/

	const struct fd_handler pop3_admin = {
		.handle_read       = pop3_admin_passive_accept,
		.handle_write      = NULL,
		.handle_close      = NULL, // nada que liberar
	};

	ss = selector_register(selector, server, &pop3_admin,
											  OP_READ, NULL);
	if(ss != SELECTOR_SUCCESS) {
		*err_msg = "registering fd";
		goto term_suscription_service;
	}
	while(!done) {
		*err_msg = NULL;
		ss = selector_select(selector);
		if(ss != SELECTOR_SUCCESS) {
			*err_msg = "serving";
			goto term_suscription_service;
		}
	}
	if(*err_msg == NULL) {
		*err_msg = "closing";
	}

term_suscription_service:

	if(selector != NULL) {
		selector_destroy(selector);
	}
	selector_close();
	destroy_suscription_pool();
	return ss;
}