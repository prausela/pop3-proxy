#include "services.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/socket.h>
       #include <sys/un.h>
int
count_fds(void)
{
    int maxfd = getdtablesize();
    int openfds;
    int fd;
    int i;

    openfds = 0;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
            return maxfd;
    close(fd);

    for (i = 0; i < maxfd; i++) {
		printf("%d\n",i);
		getchar();
            if (dup2(i, fd) != -1) {
                    openfds++;
                    close(fd);
            }
    }

    return openfds;
}
int main(void){
	int receiver[2], sender[2];
	int res = create_transformation(sender, receiver);
	if(!res){
		write(sender[0], "Tengo suenio", sizeof("Tengo suenio"));
		
	}
	return 0;
}