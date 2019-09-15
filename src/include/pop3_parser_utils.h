#ifndef POP3_PARSER_UTILS_H



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