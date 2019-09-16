#include "include/pop3_parser.h"

inline static char *get_event_type(unsigned type)
{
	switch (type)
	{
	case IGNORE:
		return "IGNORE";
	case TRAPPED:
		return "TRAPPED";
	case BUFFER_CMD:
		return "BUFFER_CMD";
	case HAS_ARGS:
		return "HAS_ARGS";
	case SET_CMD:
		return "SET_CMD";
	case BAD_CMD:
		return "BAD_CMD";
	case DAT_STUFFED:
		return "DAT_STUFFED";
	case OK_RESP:
		return "OK_RESP";
	case ERR_RESP:
		return "ERR_RESP";
	case END_SINGLELINE:
		return "END_SINGLELINE";
	}
}

int main(int argc, char *argv[])
{
	char *to_analyse = "CAPA MSG\r\n+OK\r\nBLA\r\nBLA\r\n.\r\n";

	struct parser_event *event;
	struct parser *parser = pop3_parser_init();
	while (*to_analyse != 0)
	{
			printf("%s\n", to_analyse);
			event = pop3_parser_feed(parser, *to_analyse);
			printf("TYPE: %s\nDATA: %c\nN:   %d\n", get_event_type(event->type), event->n > 0 ? (char)event->data[0] : '~', event->n);
		getchar();
		to_analyse++;
	}
	parser_destroy(parser);
	return 0;
}