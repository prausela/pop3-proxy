#include <string.h>
#include <stdio.h>
#include <netinet/in.h>

void breakpoint(void){;}

int main(void){
	struct sockaddr_in myaddr;
	int s;

	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(110);
	inet_aton("201.212.7.96", &myaddr.sin_addr.s_addr);
	breakpoint();
	printf("%d\n", myaddr.sin_port);
	return 0;
}
