#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //for exit(0);
#include <sys/socket.h>
#include <errno.h> //For errno - the error number
#include <netdb.h>	//hostent
#include <arpa/inet.h>
#include <stdbool.h>
#include "services.h"

#define IPV4 1
#define IPV6 2

int is_valid_ip_address(char *ipAddress)
{
    struct sockaddr_in sa;
    struct sockaddr_in sb;
    char aux[100];
    strcpy(aux,ipAddress);
    int result = inet_pton(AF_INET, aux, &(sa.sin_addr));
    if(result>0)
    {
      strcpy(ipAddress,aux);
      return IPV4;
    }
    result = inet_pton(AF_INET6, ipAddress, &(sb.sin_addr));
    if(result>0)
    {
      return IPV6;
    }
    return result;
}
