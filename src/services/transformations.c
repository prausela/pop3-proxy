#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //for exit(0);
#include <sys/socket.h>
#include <errno.h> //For errno - the error number
#include <netdb.h>	//hostent
#include <arpa/inet.h>
#include <stdbool.h>
#include "services.h"
#include <unistd.h>
#include  <sys/types.h>                          //
#include  <sys/wait.h>                           //
#include  <sys/stat.h>
#include <fcntl.h>

int create_transformation(int * sender_pipe, int * receiver_pipe){
  if (pipe(sender_pipe) < 0 ||pipe(receiver_pipe) < 0)
  {
    return 1;
  }
  int pid = fork();
  if(pid == 0) //Child process
  {
    dup2(sender_pipe[0],STDIN_FILENO);
    close(sender_pipe[0]);

    dup2(receiver_pipe[1],STDOUT_FILENO);
    close(receiver_pipe[1]);

    char *argv[] = {"cat", 0};
    execvp(argv[0],argv);
    return 1;
  }
  else if(pid < 0)
  {
    printf("ERROR CREATING FORK\n");
  }
  else //Parent
  {

  }
  return 0;
}
