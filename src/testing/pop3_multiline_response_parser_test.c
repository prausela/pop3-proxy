#include "../include/pop3_multiline_response_parser.h"
#include <stdio.h>

//test

inline
static char* get_event_type(unsigned type){
	switch(type){
		case IGNORE:
			return "IGNORE";
		case DAT_STUFFED:
			return "DAT_STUFFED";
		
	}
}

int main(int argc, char *argv[]){
	char *to_analyse = "LIST \r\n.\r\njasnajk";

	struct parser_event *event;
	struct parser *parser = pop3_multiline_response_parser_init();
	while(*to_analyse != 0){
		event = parser_feed(parser, *to_analyse);
        printf("%s\n",to_analyse);

		printf("TYPE: %s\nDATA: %c\nN:   %d\n", get_event_type(event->type), event->n > 0 ? (char) event->data[0] : '~', event->n);
		getchar();
		to_analyse++;
	}
	parser_destroy(parser);
	return 0;
}