#include "include/pop3.h"

#include <stdbool.h>
#include <stddef.h>

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

const char *multiline_commands[] =
	{
		"RETR",
		"TOP",
		"LANG",
		"CAPA",
		NULL,
};


bool is_in_string_array(char *what, const char **string_array)
{
	while (*string_array != NULL)
	{
		if (strcmp(what, *string_array) == 0)
		{
			return true;
		}
		string_array++;
	}
	return false;
}


int get_command_type(char *cmd)
{
	if (is_in_string_array(cmd, multiline_commands))
	{
		return MULTILINE;
	}

	if (is_in_string_array(cmd, overloaded_commands))
	{
		return OVERLOADED;
	}
	
	return SINGLELINE;
}