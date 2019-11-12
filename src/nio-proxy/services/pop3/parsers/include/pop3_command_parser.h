#ifndef POP3_COMMAND_PARSER_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM
#define POP3_COMMAND_PARSER_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM

#include "../utils/include/pop3_parser_utils.h"
#include <stdbool.h>
#include "../../../utils/include/structure_builder.h"

#define MAX_KEYWORD_LENGTH 4

struct pop3_command_builder {
	char	kwrd[MAX_KEYWORD_LENGTH+1];
	size_t	kwrd_ptr;
	bool	has_args;
};

extern struct pop3_command_builder pop3_command_builder_default;

/**	~~INIT PARSER 
 *		(Create a new parser intended for pop3 command interpretation)~~
 */
struct parser *
pop3_command_parser_init(void);

enum structure_builder_states
command_builder(buffer *b, struct parser *p, struct pop3_command_builder *cmd, bool *error);

/** 
 * PLEASE USE THE FOLLOWING PARSER_FACTORY METHODS FOR MORE OPERATIONS:
 *
 *  ~~DESTROY PARSER 
 * 		(Should be called when the parser is no longer needed)~~
 *
 *		void
 *		parser_destroy  			(struct parser *p);
 *
 *	~~RESET PARSER TO INITIAL STATE 
 *		(i.e. Parse another input)~~
 *
 *		void
 *		parser_reset    			(struct parser *p);
 *
 *	~~FEED A CHARACTER TO THE PARSER 
 *		(Below there is a by case description of behaviour)~~
 *
 *		const struct parser_event *
 *		parser_feed     			(struct parser *p, const uint8_t c);
 *
 *  ~~POP3 COMMAND PARSER~~
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
 */

/**	enum pop3_command_states {
 *		KWRD,
 *		ARG,
 *		CR,
 *		CRLF,
 *		ERR,
 * 	};
 */

// enum pop3_command_event_type {
// 	BUFFER_CMD = 1,
// 	HAS_ARGS,
// 	SET_CMD,
// 	BAD_CMD,
// //	IGNORE,			(from parser_utils)
// };

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
 *		~~EVENT~~
 *      type 	🡢 BUFFER_CMD
 *		n 		🡢 1
 *		data 	🡢 c
 *
 *		Example:
 *    	      ↓
 *			RETR 1\r\n
 *
 * KWRD  ' '🡢 ARG  :
 *
 * 		The keyword has at least one argument.
 *
 *		~~EVENT~~
 *      type 	🡢 HAS_ARGS
 *		n 		🡢 0
 *		data 	🡢 ~
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
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 *               ↓
 *  		LIST\r\n 	(Next one is \n).
 *
 *               ↓
 *  		RETR\r1\r\n (This is not a valid command).
 *
 */

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
 * ARG	 ANY🡢 ARG :
 *
 * 		We are still in the argument stage of the command.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
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
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 *                 ↓
 *  		RETR 1\r\n 	(Next one is \n).
 *
 *                ↓
 *  		TOP 1\r10\r\n (This is not a valid command).
 *
 */

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
 *		~~EVENT~~
 *      type 	🡢 SET_CMD
 *		n 		🡢 0
 *		data 	🡢 ~
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
 *		~~EVENT~~
 *      type 	🡢 BAD_CMD
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 * 		Example:
 *                 ↓
 *  		TOP 1\r10\r\n (This is not a valid command).
 *
 */

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
 * not valid thus, it is not worth checking which one it is.
 *
 *		~~EVENT~~
 *      type 	🡢 BAD_CMD
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 */

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
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 */

#endif