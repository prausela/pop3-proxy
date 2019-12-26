#ifndef GLOBAL_STRINGS_H
#define GLOBAL_STRINGS_H

#include <sys/socket.h>
#include <sys/types.h>
#include <resolv.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h> //hostent
#include <arpa/inet.h>
//#include "../include/pop3_parser.h"
//#include "../include/global_strings.h"
//#include "../include/lib.h"

#define INVALID -1
#define REQUIRED 1
#define VALID 1
#define NOT_REQUIRED 2
#define ADDRESS 3

struct pop3filter_arguments {
        char 	*error_filename;
	    char  	*client_address;
	    char 	*admin_address;
    unsigned 	 client_port;
    unsigned 	 admin_port;
    unsigned 	 origin_port;
        char	*cmd;
}  pop3filter_arguments_default;

int options_handler(const int argc, const char **argv, struct pop3filter_arguments *pop3filter_arguments);
//int command_line_parser(int argc,char **argv,char* proxy_address, char* proxy_address_ipv6, char* client_port, char* admin_address, char* admin_address_ipv6, char* admin_port, char* origin_address, char* origin_port);
//int checkArg(char *argument, int *expecting_data);

#endif