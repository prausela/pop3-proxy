#include "include/pop3_parser.h"


#define MAX_KEYWORD_COMMAND_LENGTH 4

/** ~~OVERLOADED COMMANDS~~
 * This type of commands respond differently depending on whether 
 * arguments are supplied or not.
 * 
 * {KEYWORD} {CR}{LF}		🡢 MULTILINE COMMAND
 * {KEYWORD} {ARG} {CR}{LF}	🡢 SINGLELINE COMMAND
 *
 */

const char *overloaded_commands[] = 
{
	"UIDL", 
	"LIST",
	NULL,
};

/** ~~SINGLELINE COMMANDS~~
 * This type of commands always respond in a single line with:
 * 		ON SUCCESS			🡢 +OK  ({msg}){CR}{LF}
 * 		ON FAILURE 			🡢 -ERR ({msg}){CR}{LF}
 *
 * Example:
 * C: DELE 1
 * S: +OK message 1 deleted
 * 	  ...
 * C: DELE 2
 * S: -ERR message 2 already deleted
 *
 */

const char *singleline_commands[] =
{
	"QUIT",
	"STAT",
	"LIST",
	"DELE",
	"NOOP",
	"RSET",
	"USER",
	"PASS", 
	"APOP", 
	"AUTH",
	"STLS",
	"UTF8",
	NULL,
};

/** ~~MULTILINE COMMANDS~~
 * This type of commands responds:
 * 		ON FAILURE 			🡢 SINGLELINE COMMAND
 *		ON SUCCESS 			🡣
 *			SINGLELINE COMMAND
 *			...
 *			({msg}){CR}{LF}
 *			.{CR}{LF}
 *
 * Example
 * C: RETR 1
 * S: +OK 120 octets
 * S: <the POP3 server sends the entire message here>
 * S: .
 *
 */

const char * multiline_commands[] =
{
	"RETR",
	"TOP",
	"LANG",
	"CAPA",
	NULL,
};

enum pop3_states {
		INIT,
		SL_PARSER,
		ML_PARSER,
		CMD_PARSER,
	};

	enum pop3_command_types {
		MULTILINE = 77,
		SINGLELINE = 78,
		OVERLOADED = 79,
	};

	

static void
parse_sl(struct parser_event *ret, const uint8_t c){
	ret->type 		= PARSE_SL;
	ret->n 			= 0;
}

static void
parse_dot_stuffed(struct parser_event *ret, const uint8_t c){
	ret->type 		= PARSE_DOT_STUFFED;
	ret->n 			= 0;
}

static void
parse_cmd(struct parser_event *ret, const uint8_t c){
	ret->type 		= PARSE_CMD;
	ret->n 			= 0;
}

static const struct parser_state_transition ST_INIT [] =  {
    {.when = '+',        .dest = INIT, 	.act1 = parse_sl,			},
    {.when = '-',        .dest = INIT, 	.act1 = parse_sl,			},
    {.when = ANY,        .dest = INIT, 	.act1 = parse_cmd,	},
};

static const size_t pop3_states_n [] = {
	N(ST_INIT),
	// N(SL_PARSER),
	// N(ML_PARSER),
	// N(CMD_PARSER),
};

static const struct parser_state_transition *pop3_states [] = {
    ST_INIT,
	
};

static struct parser_definition pop3_definition = {
	.states_count	= N(pop3_states),
	.states 		= pop3_states,
	.states_n 		= pop3_states_n,
	.start_state 	= INIT,
};

struct parser *curr_parser = NULL;
char cmd[] = { 0, 0, 0, 0, 0 };
size_t cmd_len = 0;
bool has_args = false;
int cmd_type;
bool answer_status = false;

struct parser *
pop3_parser_init(void){
	return parser_init(parser_no_classes(), &pop3_definition);
}

const struct parser_event *
pop3_parser_feed(struct parser *p, const uint8_t c){
	struct parser_event *event;
	if(curr_parser == NULL){
		event = parser_feed(p, c);
		//printf("event type: %d\n",event->type);
		switch(event->type){
			case PARSE_SL:
				curr_parser = pop3_singleline_response_parser_init();
				break;
			case PARSE_CMD:
				curr_parser = pop3_command_parser_init();
				break;
			default:
				return event;
		}
	}
	event = parser_feed(curr_parser, c);
	switch(event->type){
		case BUFFER_CMD:
			cmd[cmd_len] = event->data[0];
			cmd_len++;
			break;
		case HAS_ARGS:
			has_args = true;
			break;
		case SET_CMD:
			cmd_type = get_command_type(cmd);
			curr_parser = NULL;
			break;
		case OK_RESP:
			printf("%d\n", cmd_type);
			answer_status = true;
			break;
		case END_SINGLELINE:
			if(answer_status && (cmd_type == MULTILINE || cmd_type == OVERLOADED && has_args)){
				curr_parser = pop3_multiline_response_parser_init();
				ignore(event, c);
			}
			break;
	}
	return event;
}


int
get_command_type(char *cmd){
	if(is_in_string_array(cmd, multiline_commands)){
		return MULTILINE;
	}
	if (is_in_string_array(cmd, overloaded_commands)){
		return OVERLOADED;
	}
	return SINGLELINE;
}

bool
is_in_string_array(char *what, const char **string_array){
	while(*string_array != NULL){
		if(strcmp(what, *string_array) == 0){
				//printf("holaaaaaa IF TRUE\n");

			return true;
		}
		string_array++;
	}
	//printf("holaaaaaa IF FALSE\n");
	return false;
}