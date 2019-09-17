#include "../include/pop3_command_parser.h"
#include <stdio.h>

//~~TEST CASE~~

inline
static char* get_event_type(unsigned type){
	switch(type){
		case BUFFER_CMD:
			return "BUFFER_CMD";
		case HAS_ARGS:
			return "HAS_ARGS";
		case IGNORE:
			return "IGNORE";
		case SET_CMD:
			return "SET_CMD";
		case BAD_CMD:
			return "BAD_CMD";
		default:
			return NULL;
	}
}

int main(int argc, char *argv[]){
	char *to_analyse = "RETR \r\njasnxkjasnajk";
	struct parser_event *event;
	struct parser *parser = pop3_command_parser_init();
	while(*to_analyse != 0){
		event = parser_feed(parser, *to_analyse);
		printf("%s\n", to_analyse);
		printf("TYPE: %s\nDATA: %c\nN:   %d\n", get_event_type(event->type), event->n > 0 ? (char) event->data[0] : '~', event->n);
		getchar();
		to_analyse++;
	}
	parser_destroy(parser);
	return 0;
}
