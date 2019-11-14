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


int command_line_parser(int argc,char **argv,char * client_port,char * admin_port, char* proxy_address, char * origin_port, char* origin_address);
int checkArg(char *argument, int *expecting_data);

#endif
