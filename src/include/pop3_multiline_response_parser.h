#ifndef POP3_MULTILINE_RESPONSE_PARSER_H
#define POP3_MULTILINE_RESPONSE_PARSER_H

#include "parser_factory.h"
//#include "parser_utils.h"

struct parser * pop3_multiline_parser_init(void);


enum pop3_multiline_event_type {
        DAT_STUFFED,
		IGNORE,
		DAT_STUFFED_END,
	};






#endif