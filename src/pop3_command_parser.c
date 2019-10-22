#include "include/pop3_command_parser.h"

/** ~~POP3 COMMAND PARSER~~
 *
 *                 '\r'
 *        +-------------------+
 *        |   ' '       '\r'  v  '\n' +------+
 *     +----+     +---+----->+--+     |+----+|
 * +-->|KWRD|---->|ARG|      |CR|---->||CRLF||
 *     +----+     +---+      +--+     |+----+|
 *     |    ^     |   ^         |     +------+
 *     +----+     +---+      ANY|
 *      ANY        ANY          v
 *                           +-----+
 *                           |+---+|
 *                           ||ERR||
 *                           |+---+|
 *                           +-----+
 *
 */

	enum pop3_command_states {
		KWRD,
		ARG,
		CR,
		CRLF,
		ERR,
	};

/**	enum pop3_command_event_type {
 *		BUFFER_CMD = 1,
 *		HAS_ARGS,
 *		SET_CMD,
 *		BAD_CMD,
 *		IGNORE,			(from parser_utils)
 *	};
 */

/**
 *	Returns event to buffer the character of the keyword of the command.
 */

static void
buffer_cmd(struct parser_event *ret, const uint8_t c){
	ret->type 		= BUFFER_CMD;
	ret->n 			  = 1;
	ret->data[0] 	= c;
}

/**
 *	Returns event which informs that the command has arguments.
 */

static void
has_args(struct parser_event *ret, const uint8_t c){
	ret->type		= HAS_ARGS;
	ret->n 			= 0;
}

/**
 *	Returns event signaling the end of parsing the keyword of the cmd.
 */

static void
set_cmd(struct parser_event *ret, const uint8_t c){
	ret->type 		= SET_CMD;
	ret->n 			  = 0;
}

/**
 *	Returns event that signals a bad input.
 */

static void
bad_cmd(struct parser_event *ret, const uint8_t c){
	ret->type 		= BAD_CMD;
	ret->n 			  = 0;
}

/** ~~KWRD TRANSITIONS~~
 *
 *                 '\r'
 *        +-------------------+
 *        |   ' '             v
 *     +----+     +---+      +--+
 * +-->|KWRD|---->|ARG|      |CR|
 *     +----+     +---+      +--+
 *     |    ^
 *     +----+
 *      ANY
 *
 * KWRD	 ANY🡢 KWRD :
 *
 * 		We are still in the keyword stage of the command.
 *
 *		Example:
 *    	      ↓
 *			RETR 1\r\n
 *
 * KWRD  ' '🡢 ARG  :
 *
 * 		The keyword has at least one argument.
 *
 *		Example:
 *              ↓
 *  		RETR 1\r\n
 *
 * KWRD '\r'🡢 CR   :
 *
 * 		Either the next character is LF or this is not a valid
 *		command. (RFC 2449 specifies VCHAR for cmd keywords).
 *
 * 		Example:
 *               ↓
 *  		LIST\r\n 	(Next one is \n).
 *
 *               ↓
 *  		RETR\r1\r\n (This is not a valid command).
 *
 */

static const struct parser_state_transition ST_KWRD [] =  {
    {.when = ' ',        .dest = ARG,       .act1 = has_args,	},
    {.when = '\r',       .dest = CR,        .act1 = ignore,		},
    {.when = ANY,        .dest = KWRD,      .act1 = buffer_cmd,	},
};

/** ~~ARG TRANSITIONS~~
 *
 *
 *
 *                      '\r'
 *                +---+----->+--+
 *                |ARG|      |CR|
 *                +---+      +--+
 *                |   ^
 *                +---+
 *                 ANY
 *
 * ARG 	ANY🡢 ARG :
 *
 * 		We are still in the argument stage of the command.
 *
 *		Example:
 *    	          ↓
 *			RETR 100\r\n
 *
 * ARG  '\r'🡢 CR  :
 *
 * 		Either the next character is LF or this is not a valid
 *		command. (RFC 2449 specifies VCHAR for cmd arguments).
 *
 * 		Example:
 *                 ↓
 *  		RETR 1\r\n 	(Next one is \n).
 *
 *                ↓
 *  		TOP 1\r10\r\n (This is not a valid command).
 *
 */

static const struct parser_state_transition ST_ARG [] =  {
    {.when = '\r',       .dest = CR,        .act1 = ignore,		},
    {.when = ANY,        .dest = ARG,       .act1 = ignore,		},
};

/** ~~CR TRANSITIONS~~
 *
 *
 *
 *
 *
 *                               '\n' +------+
 *                           +--+     |+----+|
 *                           |CR+---->||CRLF||
 *                           +--+     |+----+|
 *                              |     +------+
 *                           ANY|
 *                              v
 *                           +-----+
 *                           |+---+|
 *                           ||ERR||
 *                           |+---+|
 *                           +-----+
 *
 * CR	'\n'🡢 CRLF :
 *
 * 		We reached the end of the command this is the ending sequence.
 *
 *		Example:
 *    	               ↓
 *			RETR 100\r\n
 *
 * CR    ANY🡢 ERR  :
 *
 * 		We have read a non-VCHAR character inside a command.
 * 		This is invalid. (RFC 2449)
 *
 * 		Example:
 *                 ↓
 *  		TOP 1\r10\r\n (This is not a valid command).
 *
 */

static const struct parser_state_transition ST_CR [] =  {
    {.when = '\n',       .dest = CRLF,      .act1 = set_cmd,	},
    {.when = ANY,        .dest = ERR,       .act1 = bad_cmd,	},
};

/** ~~ERR TRANSITIONS~~
 *
 *                           +-----+
 *                           |+---+|
 *                           ||ERR||
 *                           |+---+|
 *                           +-----+
 *
 *
 *
 * 					 ERR has no transitions
 *
 * If, for whatever reason, this state is achieved. Then the command is
 * is not valid thus, it is not worth checking which one it is.
 *
 * IF STILL ASKED TO PARSE bad_cmd ACTION WILL BE EXECUTED
 *
 */

static const struct parser_state_transition ST_ERR [] =  {
    {.when = ANY,        .dest = ERR,       .act1 = bad_cmd,	},
};

/** ~~CRLF TRANSITIONS~~
 *
 *
 *
 *                                    +------+
 *                                    |+----+|
 *                                    ||CRLF||
 *                                    |+----+|
 *                                    +------+
 *
 *
 *
 * 					          CRLF has no transitions
 *
 * When CRLF is achieved, the command has been parsed meaning that the
 * parser has no more work to do.
 *
 * IF STILL ASKED TO PARSE ignore ACTION WILL BE EXECUTED
 *
 */

static const struct parser_state_transition ST_CRLF [] =  {
    {.when = ANY,        .dest = CRLF,       .act1 = ignore,	},
};

static const size_t pop3_command_states_n [] = {
	N(ST_KWRD),
	N(ST_ARG),
	N(ST_CR),
	N(ST_CRLF),
	N(ST_ERR),
};

static const struct parser_state_transition *pop3_command_states [] = {
    ST_KWRD,
    ST_ARG,
    ST_CR,
    ST_CRLF,
    ST_ERR,
};

/** ~~POP3 COMMAND PARSER~~
 *
 *                 '\r'
 *        +-------------------+
 *        |   ' '       '\r'  v  '\n' +------+
 *     +----+     +---+----->+--+     |+----+|
 * +-->|KWRD|---->|ARG|      |CR|---->||CRLF||
 *     +----+     +---+      +--+     |+----+|
 *     |    ^     |   ^         |     +------+
 *     +----+     +---+      ANY|
 *      ANY        ANY          v
 *                           +-----+
 *                           |+---+|
 *                           ||ERR||
 *                           |+---+|
 *                           +-----+
 *
 */

static struct parser_definition pop3_command_definition = {
	.states_count	= N(pop3_command_states),
	.states 		= pop3_command_states,
	.states_n 		= pop3_command_states_n,
	.start_state 	= KWRD,
};

struct parser *
pop3_command_parser_init(void){
	return parser_init(parser_no_classes(), &pop3_command_definition);
}
