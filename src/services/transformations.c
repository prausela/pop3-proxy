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
  printf("Pipe created\n");
  int pid = fork();
  if(pid == 0) //Child process
  { //aca deberia hacer un exec y cambiar el fd
    FILE *fp = NULL;
    fp = fopen("textFile.txt" ,"a");
    fprintf(stderr,"test 1 %d\n",sender_pipe[1]);
    fprintf(stderr,"test 2 %d\n",sender_pipe[0]);

    dup2(sender_pipe[1],STDOUT_FILENO);
    dup2(receiver_pipe[0],STDIN_FILENO);
    close(sender_pipe[1]);
    close(receiver_pipe[0]);

    char *argv[] = {"cat", "textFile.txt", 0};
    fprintf(stderr,"Creating cat process\n");
    fprintf(stderr,"%d\n",sender_pipe[1]);
    //execvp(argv[0],argv);
    while(1){
      int size = 256;
      char message[size];
      read(sender_pipe[0],message,size);
      fprintf(stderr, "Lei: %s\n", message);
    }
    return 1;
  }
  else if(pid < 0)
  {
    printf("ERROR CREATING FORK\n");
  }
  else         //Parent
  {
    //wait(NULL);
  }
  return 0;
}
