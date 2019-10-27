#include "include/pop3_encrypt_parser.h"

/** ~~POP3 ENCRYPT PARSER~~
 *
 *      ANY            ANY
 *     +---+   +---------------+
 *     |   |   |               |
 *     v   |   v '\r'    '\n'  |
 *     +--------+--->+--+    +----+   '.'
 *     |DOT_DATA|    |CR|--->|CRLF|----------+
 *     +--------+<---+--+    +----+          |
 *          ^  ^  ANY                        |
 *          |  |                     ANY     |
 *          |  +-------------------------+   |
 *          |                            |   |
 *          +--------------+             |   |
 *  ANY                    |ANY          |   v
 *  +--+-----------+    +---------+    +-------+
 *  |  |CRLFDOTCRLF|<---|CRLFDOTCR|<---|CRLFDOT|<---+
 *  +->+-----------+    +---------+    +-------+
 *                  '\n'           '\r'
 */

enum pop3_encrypt_states {
				DOT_DATA = 0,
				CR,
				CRLF,
				CRLFDOT,
				CRLFDOTMANY,
	};


static void
dat_stuffed_end(struct parser_event *ret, const uint8_t c){
	ret->type		= DAT_STUFFED_END;
	ret->n 			= 0;

}

static void
dat_stuffed(struct parser_event *ret, const uint8_t c){

	ret->type		  = DAT_STUFFED;
	ret->n 		  	= 1;
	ret->data[0] 	= c;
	 // ret->next       = dat_stuffed_end;

}
static void 
dat_stuffed_dot(struct parser_event *ret, const uint8_t c){
	ret->type		  = DAT_STUFFED_DOT;
	ret->n 		  	= 1;
	ret->data[0] 	= c;
}




static const struct parser_state_transition ST_DOT_DATA [] =  {
		{.when = '\r',        .dest = CR,             .act1 = dat_stuffed,	},
		{.when = ANY,         .dest = DOT_DATA,       .act1 = dat_stuffed,	},

};



static const struct parser_state_transition ST_CR [] =  {
		{.when = '\n',       .dest = CRLF,             .act1 =  dat_stuffed,	},
		{.when = ANY,        .dest = DOT_DATA,         .act1 =  dat_stuffed,	},

};



static const struct parser_state_transition ST_CRLF [] =  {
		{.when = '.',        .dest = CRLFDOT,            .act1 = dat_stuffed_dot	},
		{.when = ANY,        .dest = DOT_DATA,           .act1 = dat_stuffed	},

};



static const struct parser_state_transition ST_CRLFDOT [] =  {
        {.when = '.',         .dest = CRLFDOTMANY,        .act1 = dat_stuffed, },
		{.when = ANY,          .dest = DOT_DATA,           .act1 =  dat_stuffed,	},
};



static const struct parser_state_transition ST_CRLFDOTMANY [] =  {
        {.when = '.',         .dest = CRLFDOTMANY,        .act1 = dat_stuffed, },
		{.when = ANY,          .dest = DOT_DATA,           .act1 =  dat_stuffed,	},
};



static const size_t pop3_encrypt_states_n [] = {
	N(ST_DOT_DATA),
	N(ST_CR),
	N(ST_CRLF),
	N(ST_CRLFDOT),
	N(ST_CRLFDOTMANY),

};

static const struct parser_state_transition *pop3_encrypt_states [] = {
		ST_DOT_DATA,
		ST_CR,
		ST_CRLF,
		ST_CRLFDOT,
		ST_CRLFDOTMANY,
};

static struct parser_definition pop3_encrypt_definition = {
	.states_count	= N(pop3_encrypt_states),
	.states 		= pop3_encrypt_states,
	.states_n 		= pop3_encrypt_states_n,
	.start_state 	= CRLFDOT,
};

struct parser *
pop3_encrypt_parser_init(void){
	return parser_init(parser_no_classes(), &pop3_encrypt_definition);
}
