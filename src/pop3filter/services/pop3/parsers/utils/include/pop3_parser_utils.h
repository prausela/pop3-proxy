#ifndef POP3_PARSER_UTILS_H
#define POP3_PARSER_UTILS_H

#include "../../../../utils/include/parser_utils.h"

enum pop3_event_type
{
	BUFFER_CMD = 3,
	HAS_ARGS,
	SET_CMD,
	BAD_CMD,
	PARSE_SL,
	PARSE_DOT_STUFFED,
	PARSE_CMD,
};

#endif