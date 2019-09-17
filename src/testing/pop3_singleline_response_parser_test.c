#include "../include/pop3_singleline_response_parser.h"
#include <stdio.h>

//~~TEST CASE~~

inline
static char* get_event_type(unsigned type){
	switch(type){
		case OK_RESP:
			return "OK_RESP";
		case ERR_RESP:
			return "ERR_RESP";
		case IGNORE:
			return "IGNORE";
		case TRAPPED:
			return "TRAPPED";
		case END_SINGLELINE:
			return "END_SINGLELINE";
		default:
			return NULL;
	}
}

int main(int argc, char *argv[]){
	char *to_analyse = "+OK \r\ngdfg";
	struct parser_event *event;
	struct parser *parser = pop3_singleline_response_parser_init();
	while(*to_analyse != 0){
		printf("%s\n%d\n", to_analyse, *to_analyse);
		event = parser_feed(parser, *to_analyse);
		getchar();
		printf("TYPE: %s\nDATA: %c\nN:   %d\n", get_event_type(event->type), event->n > 0 ? (char) event->data[0] : '~', event->n);
		getchar();
		to_analyse++;
	}
	parser_destroy(parser);
	return 0;
}
