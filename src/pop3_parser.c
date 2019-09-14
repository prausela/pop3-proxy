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
};

