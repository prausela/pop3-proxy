#ifndef POP3_SINGLELINE_RESPONSE_PARSER_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM
#define POP3_SINGLELINE_RESPONSE_PARSER_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM


#include "parser_utils.h"

/**	~~INIT PARSER 
 *		(Create a new parser intended for pop3 singleline response interpretation)~~
 */
		struct parser *
		pop3_singleline_response_parser_init	(void);

/** 
 * PLEASE USE THE FOLLOWING PARSER_FACTORY METHODS FOR MORE OPERATIONS:
 *
 *  ~~DESTROY PARSER 
 * 		(Should be called when the parser is no longer needed)~~
 *
 *		void
 *		parser_destroy  						(struct parser *p);
 *
 *	~~RESET PARSER TO INITIAL STATE 
 *		(i.e. Parse another input)~~
 *
 *		void
 *		parser_reset    						(struct parser *p);
 *
 *	~~FEED A CHARACTER TO THE PARSER 
 *		(Below there is a by case description of behaviour)~~
 *
 *		const struct parser_event *
 *		parser_feed     						(struct parser *p, const uint8_t c);
 *
 *  ~~POP3 SINGLELINE RESPONSE PARSER~~
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

/**	enum pop3_singleline_response_states {
 *		TRAP 			= 0,
 *		STAT 			= 1,
 *		OK 				   ,
 *		ERR 			   ,
 *		MSG 			   ,
 *		CR 				   ,
 *		CRLF 			   ,
 *	};
 */


	enum pop3_singleline_response_event_type {
	//	IGNORE 			= 0,
	// 	TRAPPED 		= 1,	
		OK_RESP 		= 2,
		ERR_RESP 		   ,
		END_SINGLELINE 	   ,
	};

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

#endif