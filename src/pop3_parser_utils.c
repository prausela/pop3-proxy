#include "include/pop3_parser_utils.h"
#include "include/parser_factory.h"



void
ignore(struct parser_event *ret, const uint8_t c){
	ret->type 		= IGNORE;
	ret->n 			= 0;
}

