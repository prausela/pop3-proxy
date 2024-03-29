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
#define NOT_REQUIRED 2


int command_line_parser(int argc,char **argv,char * proxy_port,char * port, char* proxy_address);
int checkArg(char *argument, int *expecting_data);

#endif
