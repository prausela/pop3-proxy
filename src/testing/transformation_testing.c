#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //for exit(0);
#include <sys/socket.h>
#include <errno.h> //For errno - the error number
#include <netdb.h>	//hostent
#include <arpa/inet.h>
#include <stdbool.h>
#include "../services/services.h"
#include <unistd.h>


int main(){
  int sender_pipe[2],receiver_pipe[2];
  int resp = create_transformation(sender_pipe,receiver_pipe);
  if(resp == 1)
  {
    printf("An error has ocurred");
    return 1;
  }
  
}
