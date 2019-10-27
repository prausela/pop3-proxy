#ifndef POP3_ENCRYPT_PARSER_H
#define POP3_ENCRYPT_PARSER_H

#include "parser_utils.h"

struct parser *pop3_encrypt_parser_init(void);

enum pop3_encrypt_event_type
{
	//IGNORE =0,
	DAT_STUFFED = 10,
	DAT_STUFFED_END,
	DAT_STUFFED_DOT,
};

#endif