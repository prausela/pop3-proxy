#include "include/pop3_multiline_response_parser.h"


// deberia hacer el dibujo, lo tengo en papel

enum pop3_multiline_response_states {
        DOT_DATA,
        CR,
        CRLF,
        CRLFDOT,
        CRLFDOTCR,
        CRLFDOTCRLF,
	};



static void
dat_stuffed_end(struct parser_event *ret, const uint8_t c){
	ret->type		= DAT_STUFFED_END;
	ret->n 			= 0;

}

static void
dat_stuffed(struct parser_event *ret, const uint8_t c){

	ret->type		= DAT_STUFFED;
	ret->n 			= 1;
    ret->data[0] 	= c;
   // ret->next       = dat_stuffed_end;
    
}


static const struct parser_state_transition ST_DOT_DATA [] =  {
    {.when = '\r',        .dest = CR,             .act1 = dat_stuffed,	},
    {.when = ANY,         .dest = DOT_DATA,       .act1 = dat_stuffed,	},

};

static const struct parser_state_transition ST_CR [] =  {
    {.when = '\n',       .dest = CRLF,              .act1 =  dat_stuffed,	},  
    {.when = ANY,        .dest = DOT_DATA,         .act1 =  dat_stuffed,	},
  
};

static const struct parser_state_transition ST_CRLF [] =  {
    {.when = '.',        .dest = CRLFDOT,            .act1 = dat_stuffed,	},
    {.when = ANY,        .dest = DOT_DATA,           .act1 =  dat_stuffed	},
  
};

static const struct parser_state_transition ST_CRLFDOT [] =  {
    {.when = '\r',        .dest = CRLFDOTCR,         .act1 = dat_stuffed,	},
    {.when = ANY,          .dest = DOT_DATA,           .act1 =  dat_stuffed,	},
  
};

static const struct parser_state_transition ST_CRLFDOTCR [] =  {
   {.when = '\n',          .dest = CRLFDOTCRLF,         .act1=   dat_stuffed ,  .act2=dat_stuffed_end,    	},
    {.when = ANY,           .dest = DOT_DATA,           .act1 =  dat_stuffed	},
  
};

static const struct parser_state_transition ST_CRLFDOTCRLF [] =  {
    {.when = ANY,        .dest = CRLFDOTCRLF,       .act1 = ignore	},
};




static const size_t pop3_multiline_response_states_n [] = {
	N(ST_DOT_DATA),
	N(ST_CR),
	N(ST_CRLF),
    N(ST_CRLFDOT),
    N(ST_CRLFDOTCR),
    N(ST_CRLFDOTCRLF),

};

static const struct parser_state_transition *pop3_multiline_response_states [] = {
    ST_DOT_DATA,
    ST_CR,
    ST_CRLF,
    ST_CRLFDOT,
    ST_CRLFDOTCR,
    ST_CRLFDOTCRLF,
};

static struct parser_definition pop3_multiline_response_definition = {
	.states_count	= N(pop3_multiline_response_states),
	.states 		= pop3_multiline_response_states,
	.states_n 		= pop3_multiline_response_states_n,
	.start_state 	= DOT_DATA,
};

struct parser *
pop3_multiline_response_parser_init(void){
	return parser_init(parser_no_classes(), &pop3_multiline_response_definition);
}










