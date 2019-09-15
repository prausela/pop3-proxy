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
  char message1[11] = "Hola mundo";
  //printf("%s\n", message1);
  printf("fd: %d\n",sender_pipe[1]);
  write(sender_pipe[1],message1,strlen(message1));
  printf("TERMINE\n");
  while(1){
    sleep(5);
    printf("Writing to pipe:\n");
    write(sender_pipe[1],message1,strlen(message1));
    sleep(5);
    int size = 256;
    char message[size];
    read(sender_pipe[0],message,size);
    printf("Lei: %s\n", message);
  }
}
