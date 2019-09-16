#ifndef POP3_MULTILINE_RESPONSE_PARSER_H
#define POP3_MULTILINE_RESPONSE_PARSER_H

#include "parser_utils.h"

struct parser *pop3_multiline_response_parser_init(void);

enum pop3_multiline_response_event_type
{
	//IGNORE =0,
	DAT_STUFFED = 10,
	DAT_STUFFED_END,
};

#endif