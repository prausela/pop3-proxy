#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //for exit(0);
#include <sys/socket.h>
#include <errno.h> //For errno - the error number
#include <netdb.h>	//hostent
#include <arpa/inet.h>
#include "services.h"
#include "../include/global_strings.h"

/*
	Get ip from domain name
*/
int hostname_to_ip(char *hostname , char *ip)
{
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	int rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_INET; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo( hostname , HTTP , &hints , &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p-> ai_next)
	{
		h = (struct sockaddr_in *) p-> ai_addr;
		strcpy(ip , inet_ntoa( h-> sin_addr ) );
	}

	freeaddrinfo(servinfo); // all done with this structure
	return 0;
}
