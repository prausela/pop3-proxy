#ifndef POP3_MULTILINE_RESPONSE_PARSER_H
#define POP3_MULTILINE_RESPONSE_PARSER_H

#include "parser_factory.h"
#include "parser_utils.h"
//#include "pop3_parser_utils.h"
struct parser * pop3_multiline_parser_init(void);

enum pop3_multiline_event_type {
		// BUFFER_CMD = 1,
		// HAS_ARGS,
		// SET_CMD,
		// BAD_CMD,
        DAT_STUFFED,
	};


enum pop3_event_type {
		BUFFER_CMD = 1,
		BAD_CMD,
		HAS_ARGS,
		SET_CMD,
	};

void
ignore(struct parser_event *ret, const uint8_t c);

static void
bad_cmd(struct parser_event *ret, const uint8_t c);

static void
set_cmd(struct parser_event *ret, const uint8_t c);

#endif