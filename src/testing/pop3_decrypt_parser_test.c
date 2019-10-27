#include "../include/pop3_encrypt_parser.h"
#include "../services/services.h"
#include<stdio.h>
#include<string.h>
#include <stdlib.h>

//test

#define TRUE 1
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

int main(int argc, char *argv[]){
    char *to_analyse = ".HOLA \r\n..como\r\n.\r\n\r\n...\r\nEstas\r\n.\rYo Bien\r\n";
    char * encrypted_message = malloc(strlen(to_analyse)*2 + 1);
    decrypt(to_analyse,encrypted_message);
    printf("%s",encrypted_message);

}
