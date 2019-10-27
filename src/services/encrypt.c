#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //for exit(0);
#include "services.h"
#include "../include/global_strings.h"
#include "../include/pop3_encrypt_parser.h"

#define TRUE  1
#define FALSE 0


inline
static char* get_event_type(unsigned type){
	switch(type){
		case IGNORE:
			return "IGNORE";
		case DAT_STUFFED:
			return "DAT_STUFFED";
		case DAT_STUFFED_END:
			return "DAT_STUFFED_END";
		default:
			return NULL;
	}
}
/**
 * encripts a given string by removing ./r/n from /r/n./r/n
 * source is the current string
 * encrypted_string is where the result of the string should be
 * */
int encrypt(char *source , char *encrypted_string)
{
	struct parser_event *event;
	struct parser *parser   = pop3_encrypt_parser_init();
    int    end              = FALSE;
    int    index            = 0;    
	while(*source != 0 && !end){
		event = parser_feed(parser, *source);
		if(strcmp(get_event_type(event->type),"DAT_STUFFED_END") == 0){
            end=TRUE;
        }else{
            encrypted_string[index++] = event->data[0];
        }
		source++;
	}
    if(end==TRUE){
        //In encrypted_string is the result
        parser_destroy(parser);
        return 1;

    }
	parser_destroy(parser);
	return 0;
}
