#include "include/pop3_multiline_response_parser.h"

/** ~~POP3 MULTILINE PARSER~~
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

enum pop3_multiline_response_states {
				CRLFDOT = 0,
				CRLFDOTCR,
				CRLFDOTCRLF,
				DOT_DATA,
				CR,
				CRLF,		
	};

#define N(x) (sizeof(x) / sizeof((x)[0]))

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
ignore(struct parser_event *ret, const uint8_t c){
	ret->type 		= DAT_STUFFED;
	ret->n 			  = 0;
}

/** ~~DOT_DATA TRANSITIONS~~
 *
 *      ANY
 *     +---+
 *     |   |
 *     v   |     '\r'
 *     +--------+--->+--+ 
 * +-->|DOT_DATA|    |CR|
 *     +--------+    +--+
 *
 * DOT_DATA   ANY🡢 DOT_DATA :
 *
 *    DOT_DATA is any CHAR, so if anything comes that is not '\r' this means we are still parsing CHAR data.
 *
 *    Example:
 *            ↓
 *      From: Paula Oseroff\r\n
 *
 * DOT_DATA  '\r'🡢 CR  :
 *
 *    We should be moving to the following line.
 *
 *    Example:
 *                         ↓
 *      From: Paula Oseroff\r\n
 *
 */


static const struct parser_state_transition ST_DOT_DATA [] =  {
		{.when = '\r',        .dest = CR,             .act1 = dat_stuffed,	},
		{.when = ANY,         .dest = DOT_DATA,       .act1 = dat_stuffed,	},

};

/** ~~CR TRANSITIONS~~
 *
 *                       '\n'
 *     +--------+    +--+    +----+
 *     |DOT_DATA|    |CR|--->|CRLF|
 *     +--------+<---+--+    +----+
 *                ANY
 *
 * CR        ANY🡢 DOT_DATA :
 *
 *    If a '\n' is not present then it is not the end of the line.
 *
 *    Example:
 *                           ↓
 *      From: Paula Oseroff\rNew\n
 *
 * CR       '\n'🡢 CRLF  :
 *
 *    We are moving to the next line.
 *
 *    Example:
 *                            ↓
 *      From: Paula Oseroff\r\n
 *
 */

static const struct parser_state_transition ST_CR [] =  {
		{.when = '\n',       .dest = CRLF,             .act1 =  dat_stuffed,	},
		{.when = ANY,        .dest = DOT_DATA,         .act1 =  dat_stuffed,	},

};

/** ~~CRLF TRANSITIONS~~
 *
 *                     ANY
 *             +---------------+
 *             |               |
 *             v               |
 *     +--------+            +----+   '.'
 *     |DOT_DATA|            |CRLF|----------+
 *     +--------+            +----+          |
 *                                           |
 *                                           |
 *                                           |
 *                                           |
 *                                           |
 *                                           v
 *                                     +-------+
 *                                     |CRLFDOT|<---+
 *                                     +-------+
 *
 * CRLF			 '.'🡢 CRLFDOT :
 *
 *    This could be the end of the message or just a . followed by more
 *		DOT STUFFED.
 *
 *    Example:
 *                           
 *      From: Paula Oseroff\r\n
 *			DATA\r\n
 *			I'm Mr. Meeseeks, look at me!\r\n
 *			↓
 *			.\r\n
 *
 * CRLF      ANY🡢 DOT_DATA  :
 *
 *    We are now in the following line.
 *
 *    Example:
 *                            
 *      From: Paula Oseroff\r\n
 *			↓
 *			DATA\r\n
 *			I'm Mr. Meeseeks, look at me!\r\n
 *			.\r\n
 *
 */

static const struct parser_state_transition ST_CRLF [] =  {
		{.when = '.',        .dest = CRLFDOT,            .act1 = dat_stuffed,	},
		{.when = ANY,        .dest = DOT_DATA,           .act1 = dat_stuffed	},

};

/** ~~CRLFDOT TRANSITIONS~~
 *
 *     +--------+
 *     |DOT_DATA|
 *     +--------+
 *             ^
 *             |                     ANY
 *             +-------------------------+
 *                                       |
 *                                       |
 *                                       |
 *                      +---------+    +-------+
 *                      |CRLFDOTCR|<---|CRLFDOT|<---+
 *                      +---------+    +-------+
 *                                 '\r'
 *
 * CRLFDOT '\r'🡢 CRLFDOTCR :
 *
 *    If next caracter is '\n' then we have read all the DOT STUFFED data.
 *		There are two scenarios, we are at the begining of the DOT STUFFED
 *		(the previous \r\n belong to the singleline), or we follow a \r\n.
 *
 *    Example:
 *
 *				↓
 *			.\r\n
 *                           
 *      From: Paula Oseroff\r\n
 *			DATA\r\n
 *			I'm Mr. Meeseeks, look at me!\r\n
 *		  	↓
 *			.\r\n
 *
 *
 * CRLFDOT   ANY🡢 DOT_DATA  :
 *
 *    The dot did not mean end of message.
 *
 *    Example:
 *                            
 *      From: Paula Oseroff\r\n			
 *			DATA\r\n
 *			I'm Mr. Meeseeks, look at me!\r\n
 *			 ↓
 *			...Yeah, sure\r\n
 *			./r/n
 *
 */

static const struct parser_state_transition ST_CRLFDOT [] =  {
		{.when = '\r',         .dest = CRLFDOTCR,         .act1 = dat_stuffed,	},
		{.when = ANY,          .dest = DOT_DATA,           .act1 =  dat_stuffed,	},
};

/** ~~CRLFDOTCR TRANSITIONS~~
 *
 *     +--------+
 *     |DOT_DATA|
 *     +--------+
 *          ^
 *          |
 *          |
 *          |
 *          +--------------+
 *                         |ANY
 *     +-----------+    +---------+
 *     |CRLFDOTCRLF|<---|CRLFDOTCR|
 *     +-----------+    +---------+
 *                  '\n'
 *
 * CRLFDOTCR 	'\n'🡢 CRLFDOTCRLF :
 *
 *    We have reached the end of the DOT STUFFED.
 *
 *    Example:
 *
 *				  ↓
 *			.\r\n
 *                           
 *      From: Paula Oseroff\r\n
 *			DATA\r\n
 *			I'm Mr. Meeseeks, look at me!\r\n
 *		  	  ↓
 *			.\r\n
 *
 *
 * CRLFDOTCR   ANY🡢 DOT_DATA  :
 *
 *    We were not jumping lines.
 *
 *    Example:
 *                            
 *      From: Paula Oseroff\r\n			
 *			DATA\r\n
 *			I'm Mr. Meeseeks, look at me!\r\n
 *			   ↓
 *			.\rWassaaaa\n
 *			.\r\n
 *
 */

static const struct parser_state_transition ST_CRLFDOTCR [] =  {
	 {.when = '\n',          .dest = CRLFDOTCRLF,         .act1=   dat_stuffed ,  .act2=dat_stuffed_end,    	},
		{.when = ANY,          .dest = DOT_DATA,            .act1 =  dat_stuffed	},

};

/** ~~CRLFDOTCRLF TRANSITIONS~~
 *
 *   ANY
 *  +--+-----------+
 *  |  |CRLFDOTCRLF|
 *  +->+-----------+
 *
 * CRLFDOTCRLF   ANY🡢 CRLFDOTCRLF  :
 *
 *    The DOT STUFFED had already ended and thus this is
 *		not DOT STUFFED.
 *
 *    Example:
 *                            
 *      From: Paula Oseroff\r\n			
 *			DATA\r\n
 *			I'm Mr. Meeseeks, look at me!\r\n
 *			...Yeah, sure\r\n
 *			./r/n
 * 			↓
 *			Daikiri en la playa ;).\r\n
 *
 */

static const struct parser_state_transition ST_CRLFDOTCRLF [] =  {
		{.when = ANY,        .dest = CRLFDOTCRLF,       .act1 = ignore	},
};




static const size_t pop3_multiline_response_states_n [] = {
	//N(ST_TRAP),
	N(ST_CRLFDOT),
	N(ST_CRLFDOTCR),
	N(ST_CRLFDOTCRLF),
	N(ST_DOT_DATA),
	N(ST_CR),
	N(ST_CRLF),
	

};

static const struct parser_state_transition *pop3_multiline_response_states [] = {
		//ST_TRAP,
		ST_CRLFDOT,
		ST_CRLFDOTCR,
		ST_CRLFDOTCRLF,
		ST_DOT_DATA,
		ST_CR,
		ST_CRLF,
		
};

static struct parser_definition pop3_multiline_response_definition = {
	.states_count	= N(pop3_multiline_response_states),
	.states 		= pop3_multiline_response_states,
	.states_n 		= pop3_multiline_response_states_n,
	.start_state 	= CRLFDOT,
};

struct parser *
pop3_multiline_response_parser_init(void){
	return parser_init(parser_no_classes(), &pop3_multiline_response_definition);
}


enum consumer_state
pop3_multiline_response_checker(buffer *b, struct parser *p, bool *error){
	uint8_t c;
    struct parser_event *e;
    size_t  count;
    //printf("Giorno\n");
    uint8_t *ptr = buffer_read_ptr(b, &count);
    while(count > 0){
        c = *ptr;
        printf("Buccelatti\n");
        printf("%c %d\n", c, c);
        e = parser_feed(p, c);
        printf("%c %d\n", c, c);
        printf("Im Mario\n");
        fflush(stdout);
        if(e->next != NULL && e->next->type == DAT_STUFFED_END){
        	return FINISHED_CONSUMING;
        }
        ptr++;
        count--;
    }

    return CONSUMING;
}