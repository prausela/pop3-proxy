#include "include/pop3_singleline_response_parser.h"

/** ~~POP3 SINGLELINE RESPONSE PARSER~~
 *
 *           '+'  +--+  '\r'
 *          +---->|OK|------+
 *          |     +--+      |
 *          |      |ANY     |
 *          |      |        |
 *          |      v   '\r' v  '\n' +------+
 *     +----+  +>+---+---->+--+     |+----+|
 * +-->|STAT|  | |MSG|     |CR|---->||CRLF||
 *     +----+  +-+---+<----+--+     |+----+|
 *      |   | ANY  ^   ANY  ^       +------+
 *      |   |      |        |
 *   ANY|   |      |ANY     |
 *      |   |     +---+     |
 *      v   +---->|ERR|-----+
 *    +----+ '-'  +---+ '\r'
 *    |TRAP|
 *    +----+
 *    |    ^
 *    +----+
 *     ANY
 *
 */

	enum pop3_singleline_response_states {
	//	TRAP 			= 0,
		STAT 			= 1,
		OK 				= 2,
		ERR 			= 3,
		MSG 			= 4,
		CR 				= 5,
		CRLF 			= 6,

	};

/**	enum pop3_singleline_response_event_type {
 *		IGNORE 			= 0,
 *	 	TRAPPED 		= 1,
 *		OK_RESP 		= 2,
 *		ERR_RESP 		   ,
 *		END_SINGLELINE 	   ,
 *	};
 */

static void
ok_resp(struct parser_event *ret, const uint8_t c){
	ret->type 	= OK_RESP;
	ret->n			= 0;
}

static void
err_resp(struct parser_event *ret, const uint8_t c){
	ret->type 	= ERR_RESP;
	ret->n 			= 0;
}

static void
end_singleline(struct parser_event *ret, const uint8_t c){
	ret->type 	= END_SINGLELINE;
	ret->n 			= 0;
}

/** ~~STAT TRANSITIONS~~
 *
 *           '+'  +--+
 *          +---->|OK|
 *          |     +--+
 *          |
 *          |
 *          |
 *     +----+
 * +-->|STAT|
 *     +----+
 *      |   |
 *      |   |
 *   ANY|   |
 *      |   |     +---+
 *      v   +---->|ERR|
 *    +----+ '-'  +---+
 *    |TRAP|
 *    +----+
 *
 * STAT	 '+'🡢 OK 	:
 *
 * 		This implies a positive response.
 *
 * 		Note: 	This assumes '+' is always followed by 'OK' and thus
 * 				'+\r\n' would be accepted as a positive response. By RFC
 * 				2449 answer like this would not be sent.
 *
 *		~~EVENT~~
 *      type 	🡢 OK_RESP
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 *		Example:
 *    	    ↓
 *			+OK dewey POP3 server signing off\r\n
 *
 * STAT  '-'🡢 ERR 	:
 *
 * 		This implies a negative response.
 *
 * 		~~EVENT~~
 *      type 	🡢 ERR_RESP
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 * 			  ↓
 * 			+\r\n
 *
 * STAT  ANY🡢 TRAP 	:
 *
 * 		Responses must be greeting/singleline/multiline/capa-res following
 * 		RFC 2449. If not it should not be a valid response.
 *
 * 		~~EVENT~~
 *      type 	🡢 TRAPPED
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 * 			↓
 * 			Hi\r\n
 *
 */

static const struct parser_state_transition ST_STAT [] = {
	{.when = '+',		.dest = OK,			.act1 = ok_resp, 		},
	{.when = '-', 		.dest = ERR, 		.act1 = err_resp, 		},
	{.when = ANY, 		.dest = TRAP, 		.act1 = trap, 			},
};

/** ~~OK TRANSITIONS~~
 *
 *
 *                  +--+  '\r'
 *                  |OK|------+
 *                  +--+      |
 *                   |        |
 *                   |ANY     |
 *                   v        v
 *                 +---+     +--+
 *                 |MSG|     |CR|
 *                 +---+     +--+
 *
 *
 * OK	  ANY🡢 MSG 	:
 *
 * 		This is the text that follows the '+', typically 'OK' and a
 * 		SP text.
 *
 * 		Note: 	This assumes '+' is always followed by 'OK' and thus
 * 				'+\r\n' would be accepted as a positive response. By RFC
 * 				2449 answer like this would not be sent.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 *		Example:
 *    	     ↓
 *			+OK dewey POP3 server signing off\r\n
 *
 * OK 	'\r'🡢 CR 	:
 *
 * 		This means we have reached the end of the response. Although
 * 		this should not happen it is considered.
 *
 * 		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 * 			  ↓
 * 			+\r\n
 *
 */

static const struct parser_state_transition ST_OK [] = {
	{.when = '\r', 		.dest = CR, 		.act1 = ignore, 		},
	{.when = ANY, 		.dest = MSG, 		.act1 = ignore, 		},
};

/** ~~ERR TRANSITIONS~~
 *
 *                 +---+     +--+
 *                 |MSG|     |CR|
 *                 +---+     +--+
 *                   ^        ^
 *                   |ANY     |
 *                   |        |
 *                  +---+     |
 *                  |ERR|-----+
 *                  +---+ '\r'
 *
 * ERR	  ANY🡢 ERR 	:
 *
 * 		This is the text that follows the '-', typically 'ERR' and a
 * 		SP text.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 *		Example:
 *    	     ↓
 *			-ERR no such message\r\n
 *
 * ERR 	 '\r'🡢 CR 	:
 *
 * 		This means we have reached the end of the response. The SP text
 * 		could be empty. This should not be happening but it is considered.
 *
 * 		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 * 			  ↓                   ↓
 * 			-\r\n
 *
 */

static const struct parser_state_transition ST_ERR [] = {
	{.when = '\r', 		.dest = CR, 		.act1 = ignore, 		},
	{.when = ANY, 		.dest = MSG, 		.act1 = ignore, 		},
};

/** ~~MSG TRANSITIONS~~
 *
 *                       '\r'
 *               +>+---+---->+--+
 *               | |MSG|     |CR|
 *               +-+---+     +--+
 *              ANY
 *
 * MSG	  ANY🡢 MSG 	:
 *
 * 		We are still in the SP text.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 *		Example:
 *    	           ↓
 *			+OK dewey POP3 server signing off\r\n
 *
 * MSG 	'\r'🡢 CR 	:
 *
 * 		This means we have reached the end of the response.
 *
 * 		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 * 			    ↓
 * 			+OK\r\n (This could be the response of NOOP command)
 * 											  ↓
 *			+OK dewey POP3 server signing off\r\n
 *
 */

static const struct parser_state_transition ST_MSG [] = {
	{.when = '\r', 		.dest = CR, 		.act1 = ignore, 		},
	{.when = ANY, 		.dest = MSG, 		.act1 = ignore, 		},
};

/** ~~CR TRANSITIONS~~
 *
 *                               '\n' +------+
 *                 +---+     +--+     |+----+|
 *                 |MSG|     |CR|---->||CRLF||
 *                 +---+<----+--+     |+----+|
 *                       ANY          +------+
 *
 * CR	 '\n'🡢 CRLF	:
 *
 * 		We reached the end of the response, this is the ending sequence.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 *		Example:
 *    	                           ↓
 *			-ERR no such message\r\n
 *
 * CR	 '\r'🡢 MSG	:
 *
 * 		This means the \r was not part of the ending pair but rather
 * 		part of the message following the status.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 *		Example:
 *    	            ↓
 *			-ERR '\r' command not found\n
 */

static const struct parser_state_transition ST_CR [] = {
	{.when = '\n', 		.dest = CRLF, 		.act1 = end_singleline, },
	{.when = ANY, 		.dest = MSG, 		.act1 = ignore,			},
};

/** ~~CRLF TRANSITIONS~~
 *
 *                                    +------+
 *                                    |+----+|
 *                                    ||CRLF||
 *                                    |+----+|
 *                                    +------+
 *
 *
 * 						 		CRLF has no transitions
 *
 * When CRLF is achieved, the command has been parsed meaning that the
 * parser has no more work to do.
 *
 * IF STILL ASKED TO PARSE ignore ACTION WILL BE EXECUTED
 *
 */

static const struct parser_state_transition ST_CRLF [] = {
	{.when = ANY, 		.dest = CRLF, 		.act1 = ignore, 		},
};


static const size_t pop3_singleline_response_states_n [] = {
	N(ST_TRAP),
	N(ST_STAT),
	N(ST_OK),
	N(ST_ERR),
	N(ST_MSG),
	N(ST_CR),
	N(ST_CRLF),
};

static const struct parser_state_transition *pop3_singleline_response_states [] = {
  ST_TRAP,
  ST_STAT,
	ST_OK,
	ST_ERR,
	ST_MSG,
	ST_CR,
	ST_CRLF,
};

/** ~~POP3 SINGLELINE RESPONSE PARSER~~
 *
 *           '+'  +--+  '\r'
 *          +---->|OK|------+
 *          |     +--+      |
 *          |      |ANY     |
 *          |      |        |
 *          |      v   '\r' v  '\n' +------+
 *     +----+  +>+---+---->+--+     |+----+|
 * +-->|STAT|  | |MSG|     |CR|---->||CRLF||
 *     +----+  +-+---+<----+--+     |+----+|
 *      |   | ANY  ^   ANY  ^       +------+
 *      |   |      |        |
 *   ANY|   |      |ANY     |
 *      |   |     +---+     |
 *      v   +---->|ERR|-----+
 *    +----+ '-'  +---+ '\r'
 *    |TRAP|
 *    +----+
 *    |    ^
 *    +----+
 *     ANY
 *
 */

static struct parser_definition pop3_singleline_response_definition = {
	.states_count	= N(pop3_singleline_response_states),
	.states 		= pop3_singleline_response_states,
	.states_n 		= pop3_singleline_response_states_n,
	.start_state 	= STAT,
};

enum structure_builder_states
pop3_singleline_response_builder(buffer *b, struct parser *p, struct pop3_singleline_response_builder *resp, bool *error){
	uint8_t c;
	struct parser_event *e;
	size_t  count;
	uint8_t *ptr = buffer_read_ptr(b, &count);
	printf("MIIIII %d\n", count);
	while(count > 0){
		printf("Oh no\n");
		c = *ptr;
		printf("Oh duhh\n");
		e = parser_feed(p, c);
		printf("char : %d\n", c);
		switch(e->type){
			case OK_RESP:
				printf("OK_RESP\n");
				resp->status = STATUS_OK;
				break;
			case ERR_RESP:
				printf("ERR_RESP\n");
				resp->status = STATUS_ERR;
				break;
			case END_SINGLELINE:
				printf("END_SINGLELINE\n");
				return BUILD_FINISHED;
			case TRAPPED:
				printf("TRAPPED\n");
				*error = 1; 
				return BUILD_FAILED;
			default:
				break;
		}
		ptr++;
		count--;
	}
	
	return BUILDING;
}
/*
bool 
pop3_singleline_parser_is_done(const struct parser_event *event, bool *errored) {
    bool ret;
    switch (event->type) {
        case TRAPPED:
            if (0 != errored) {
                *errored = true;
            }
        case END_SINGLELINE:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
   return ret;
}

struct parser_event *
pop3_singleline_parser_consume(buffer *b, struct parser *p, bool *errored) {
	return parser_consume(b, p, errored, pop3_singleline_parser_is_done);
}*/

struct parser *
pop3_singleline_response_parser_init(void){
	return parser_init(parser_no_classes(), &pop3_singleline_response_definition);
}
