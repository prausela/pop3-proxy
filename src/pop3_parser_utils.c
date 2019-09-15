#include "include/pop3_parser_utils.h"
#include "include/parser_factory.h"



void
ignore(struct parser_event *ret, const uint8_t c){
	ret->type 		= IGNORE;
	ret->n 			= 0;
}

/**
 *	Returns event that signals a bad input.
 */

static void
bad_cmd(struct parser_event *ret, const uint8_t c){
	ret->type 		= BAD_CMD;
	ret->n 			= 0;
}

/**
 *	Returns event signaling the end of parsing the keyword of the cmd.
 */

static void
set_cmd(struct parser_event *ret, const uint8_t c){
	ret->type 		= SET_CMD;
	ret->n 			= 0;
}