#include "include/pop3_multiline_response_parser.h"

// deberia hacer el dibujo, lo tengo en papel

enum pop3_multiline_response_states {
		STATUS,
        OK,
        CR,
        CRLF,
        CRLFDOT,
        CRLFDOTCR,
        CRLFDOTCRLF,
        ERR,
	};

static void
dat_stuffed(struct parser_event *ret, const uint8_t c){
	ret->type		= DAT_STUFFED;
	ret->n 			= 0;
}

static const struct parser_state_transition ST_STATUS [] =  {
    {.when = '+',        .dest = OK,       .act1 = dat_stuffed,	},
    {.when = ANY,        .dest = ERR,       .act1 = bad_cmd,	},

};

static const struct parser_state_transition ST_OK [] =  {
    {.when = '\r',        .dest = CR,        .act1 = dat_stuffed,	},
    {.when = ANY,         .dest = ERR,       .act1 = bad_cmd,	},

};

static const struct parser_state_transition ST_CR [] =  {
    {.when = '\n',       .dest = CRLF,       .act1 = ignore,	},  
    {.when = ANY,        .dest = OK,         .act1 = dat_stuffed,	},
  
};

static const struct parser_state_transition ST_CRLF [] =  {
    {.when = '.',        .dest = CRLFDOT,      .act1 = ignore,	},
    {.when = ANY,        .dest = OK,           .act1 = dat_stuffed,	},
  
};

static const struct parser_state_transition ST_CRLFDOT [] =  {
    {.when = '\r',        .dest = CRLFDOTCR,      .act1 = ignore,	},
    {.when = ANY,          .dest = OK,           .act1 = dat_stuffed,	},
  
};

static const struct parser_state_transition ST_CRLFDOTCR [] =  {
   {.when = '\n',          .dest = CRLFDOTCRLF,        .act1 = ignore,	},
    {.when = ANY,           .dest = OK,                 .act1 = dat_stuffed,	},
  
};

static const struct parser_state_transition ST_CRLFDOTCRLF [] =  {
    {.when = ANY,        .dest = CRLFDOTCRLF,       .act1 = ignore,	},
};

static const struct parser_state_transition ST_ERR [] =  {
    {.when = ANY,        .dest = ERR,       .act1 = bad_cmd,	},
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t pop3_multiline_response_states_n [] = {
	N(ST_STATUS),
	N(ST_OK),
	N(ST_CR),
	N(ST_CRLF),
    N(ST_CRLFDOT),
    N(ST_CRLFDOTCR),
    N(ST_CRLFDOTCRLF),
	N(ST_ERR),    

};

static const struct parser_state_transition *pop3_multiline_response_states [] = {
    ST_STATUS,
    ST_OK,
    ST_CR,
    ST_CRLF,
    ST_CRLFDOT,
    ST_CRLFDOTCR,
    ST_CRLFDOTCRLF,
    ST_ERR,
};

static struct parser_definition pop3_multiline_definition = {
	.states_count	= N(pop3_multiline_response_states),
	.states 		= pop3_multiline_response_states,
	.states_n 		= pop3_multiline_response_states_n,
	.start_state 	= STATUS,
};

struct parser *
pop3_multiline_parser_init(void){
	return parser_init(parser_no_classes(), &pop3_multiline_definition);
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





