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
        case DAT_STUFFED_DOT:
            return "DAT_STUFFED_DOT";
		default:
			return NULL;
	}
}
/**
 * encripts a given string by removing ./r/n from /r/n./r/n
 * source is the current string
 * encrypted_string is where the result of the string should be
 * */
int decrypt(char *source , char *decrypted_string)
{
	struct parser_event *event;
	struct parser *parser   = pop3_encrypt_parser_init();
    int    end              = FALSE;
    int    index            = 0;   
    printf("%s\n", source);
    printf("--------------------------------------------\n");
    if(source[index]=='.')
    {
        decrypted_string[index++] = '.';
    }
	while(*source != 0 && !end){
		event = parser_feed(parser, *source);
        /*printf("%s\n", source);
		printf("TYPE: %s\nDATA: %c\nN:   %d\n", get_event_type(event->type), event->n > 0 ? (char) event->data[0] : '~', event->n);
		getchar();*/
		if(strcmp(get_event_type(event->type),"DAT_STUFFED_DOT") == 0){
            decrypted_string[index++] = '.';
        }
        decrypted_string[index++] = event->data[0];
		source++;
	}
        decrypted_string[index++] = '.';
        decrypted_string[index++] = '\r';
        decrypted_string[index++] = '\n';
        parser_destroy(parser);
        return 0;
}
