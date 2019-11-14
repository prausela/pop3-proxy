#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include "common.h"

#define SOCKETS "sock"
#define TRANSFORMATION "trans"
#define METRICS "metrics"
#define CONFIGURATION "config"

#define STATUS "status"
#define MOD "mod"
#define BYTES "bytes\n"
#define ACCESS "access\n"
#define CONECTIONS "conections\n"

#define ORIGIN "origin"
#define LOCAL "local"
#define PORT "port"
#define ADDRESS "address"
#define ALL "all"
#define GENERAL "general"
#define MIME "mime"
#define BAN "ban"
#define REPLACE "replace"

char encode_request(char** argv, int argc, char** parameters, int* size);
int show_byte(char to_decode);
char sock_encoder(char** argv, int argc, char byte, char * parameters[], int* size);
char trans_encoder(char** argv, int argc, char byte, char* parameters[], int* size);
char metrics_encoder(char** argv, int argc, char byte, char* parameters[], int* size);
char config_encoder(char** argv, int argc, char byte, char* parameters[], int* size);

int main(int argc, char ** argv)
{
  int connSock, in, i, ret, flags;
  struct sockaddr_in servaddr;
  struct sctp_status status;
  struct sctp_sndrcvinfo sndrcvinfo;
  struct sctp_event_subscribe events;
  struct sctp_initmsg initmsg;
  char buffer[MAX_BUFFER+1];
  int port=9090;
  time_t currentTime;

  /* Create an SCTP TCP-Style Socket */
  connSock = socket( AF_INET, SOCK_STREAM, IPPROTO_SCTP );

  /* Specify that a maximum of 5 streams will be available per socket */
  memset( &initmsg, 0, sizeof(initmsg) );
  initmsg.sinit_num_ostreams = 5;
  initmsg.sinit_max_instreams = 5;
  initmsg.sinit_max_attempts = 4;
  ret = setsockopt( connSock, IPPROTO_SCTP, SCTP_INITMSG,
                     &initmsg, sizeof(initmsg) );

  if(argc>1){
    port=atoi(argv[1]);
    if(port==0){
      printf("Ingrese un numero de puerto valido\n");
      return 1;
    }
  }

  /* Specify the peer endpoint to which we'll connect */
  bzero( (void *)&servaddr, sizeof(servaddr) );
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = inet_addr( "127.0.0.1" );

  printf("Connecting to port %d\n",port );

  /* Connect to the server */
  ret = connect( connSock, (struct sockaddr *)&servaddr, sizeof(servaddr) );
  if(ret==-1){
    printf("Error al conectarse\n");
  }
  /* Enable receipt of SCTP Snd/Rcv Data via sctp_recvmsg */
  memset( (void *)&events, 0, sizeof(events) );
  events.sctp_data_io_event = 1;
  ret = setsockopt( connSock, SOL_SCTP, SCTP_EVENTS,
                     (const void *)&events, sizeof(events) );

  /* Read and emit the status of the Socket (optional step) */
  in = sizeof(status);
  ret = getsockopt( connSock, SOL_SCTP, SCTP_STATUS,
                     (void *)&status, (socklen_t *)&in );

  // Defino packages
  char ebyte=0;
  char** parameters;
  int size=0;

  if((parameters=malloc(5*sizeof(char*)))==NULL){
      printf("Error. No se pudo crear memoria para parameters[].\n");
      return 1;
  }
  int pointer=0;
  char temporary_buffer[1000];
  char *token;
  char ** input_buffer;

  if((input_buffer=malloc(6*sizeof(char*)))==NULL){
      printf("Error. No se pudo crear memoria para input_buffer[].\n");
      return 1;
  }

  ////////////////

  while(1){
    printf(":> ");
    fgets(temporary_buffer,1000,stdin);
    pointer=0;

    token = strtok(temporary_buffer, " ");
    input_buffer[pointer++]=token;
    /* walk through other tokens */
    while( token != NULL ) {
       token = strtok(NULL, " ");
       if(token != NULL){
         input_buffer[pointer++]=token;
       }
    }

    //printf("The input words are: ");
    //for(i=0; i<pointer; i++){
    //  printf("%s ",input_buffer[i]);
    //}
    //printf("\n");
    //printf("The quantity of words in the input are: %d\n",pointer);

    ebyte=encode_request(input_buffer, pointer, parameters, &size);


    if(ebyte==-1){
        printf("Invalid command\n");
    }
    else{
        printf("PROTOCOL RESPONSE: THE eBYTE IS ");
        show_byte(ebyte);

        pointer=2;
        buffer[0]=ebyte;
        buffer[1]=size;
        if (size>0){
            printf("The quantity of parameters are: %d\n",size );
            printf("The parameters are: ");
            for(i=0; i<size; i++){
                printf(" %s ", parameters[i]);
            }
            pointer=2;
            for (i=0; i<size; i++){
                strcpy(buffer+pointer, parameters[i]);
                pointer+=strlen(parameters[i])+1;
                //printf("El tamaño de el parametro %s es %d\n",parameters[i], strlen(parameters[i]));
            }

            printf("\n");
            printf("The parameters copied to buffer are: \n");
            pointer=2;

            pointer=2;
            for(i=0; i<size; i++){
                printf("%d ) ",i+1);
                printf("%s\n",buffer+pointer);
                pointer+=strlen(parameters[i])+1;
            }

        }


        ret = sctp_sendmsg( connSock, (void *)buffer, (size_t)MAX_BUFFER,
                            NULL, 0, 0, 0, LOCALTIME_STREAM, 0, 0 );



        in = sctp_recvmsg( connSock, (void *)buffer, sizeof(buffer),
                            (struct sockaddr *)NULL, 0, &sndrcvinfo, &flags );
        if (in > 0) {
        buffer[in] = 0;
        if(buffer[0]==0x00){
            printf("Bad request response\n");
        }
        else{
            printf("Proxy says: all OK\n");
        }
        //if (sndrcvinfo.sinfo_stream == LOCALTIME_STREAM) {
        //} else if (sndrcvinfo.sinfo_stream == GMT_STREAM) {
            //printf("TODO MAL\n", buffer);
        //}
        }
    }
    bzero((char *)&buffer, sizeof(buffer));
    size=0;
  }

  /* Close our socket and exit */
  close(connSock);

  return 0;
}

char encode_request(char** argv, int argc, char** parameters, int* size){
    char byte=0;    // El byte debe comenzar en 0
    if(strcmp(argv[0],SOCKETS)==0){
        printf("It is a socket operation. Entering SOCKETS encoder.....\n");
        // Mascara 00
        byte=sock_encoder(argv, argc, byte, parameters, size);
    }
    else if(strcmp(argv[0],TRANSFORMATION)==0){
        printf("It is a transformation operation. Entering TRANSFORMATION encoder.....\n");
        byte=(byte | 0x40);
        byte=trans_encoder(argv, argc, byte, parameters, size);
    }
    else if(strcmp(argv[0],METRICS)==0){
        printf("It is a metrics operation. Entering METRICS encoder.....\n");
        byte=(byte | 0x80);
        byte=metrics_encoder(argv, argc, byte, parameters, size);
    }
    else if(strcmp(argv[0],CONFIGURATION)==0){
        printf("It is a configuration operation. Entering CONFIGURATION encoder.....\n");
        byte=(byte | 0xC0);
        byte=config_encoder(argv, argc, byte, parameters, size);
    }
    else{
        return -1;
    }
    return byte;
}


int show_byte(char to_decode){
    char a = to_decode;
    int i;
    for (i = 0; i < 8; i++) {
      printf("%d", !!((a << i) & 0x80));
    }
    printf("\n");
    return 1;
}

char sock_encoder(char** argv, int argc, char byte, char* parameters[], int* size){
    if (strcmp(argv[1],STATUS)==0){
        // Dejo esos 2 bits en 0, no requiere cambio
        // Chequeo proximo parametro
        if(argc<6){
            printf("Error. Syntax: <sock> <status> <origin/local>\n");
        }
        else{
            if (strcmp(argv[2],ORIGIN)==0){
                // Tampoco cambio bits.
            }
            else if ((strcmp(argv[2],LOCAL)==0)){
                byte=byte|0x04;
            }
            else{
                printf("Error. Not a valid command for status.\n");
                return -1;
            }
        }
    }

    else if(strcmp(argv[1],MOD)==0){
        if(argc!=5){ //Arreglar por si pasan mas parametros de los que se debe
            printf("Error. Sintaxis: <sock> <mod> <origin/local> <port/address> <parameter>\n" );
        }
        else{
            byte=byte|0x10;
            if (strcmp(argv[2],LOCAL)==0){
                byte=byte|0x04;
            }
            else if ((strcmp(argv[2],ORIGIN)==0)){
                // No cambio bits
            }
            else{
                printf("Error. Not a valid command for mod.\n");
                return -1;  // OJO CON EL VALOR DE ERROR
            }

            // Si no hubo error, continúa

            if (strcmp(argv[3],ADDRESS)==0){
                byte=byte|0x01;
            }
            else if ((strcmp(argv[3],PORT)==0)){
                // No cambio bits
            }
            else{
                printf("Error. Not a valid command for mod.\n");
                return -1;  // OJO CON EL VALOR DE ERROR
            }

            parameters[*size]=argv[4];
            (*size)++;
        }
    }

    return byte;
}



char trans_encoder(char** argv, int argc, char byte, char* parameters[], int* size){
  if (strcmp(argv[1],STATUS)==0){   // 01.00.xx.xx = STATUS
      if(argc>2){
          printf("Error. Syntax: <trans> <status>\n");
          return -1;
      }
      else{
        // Dejo el byte como está
      }
  }

  else if(strcmp(argv[1],MOD)==0){  //  01.01.xx.xx = MOD
      if(argc<5){ //Arreglar por si pasan mas parametros de los que se debe
          printf("Error. Syntax: <trans> <mod> <general/mime> <operation> <parameter>\n" );
      }
      else{
          byte=byte|0x10;
          if (strcmp(argv[2],GENERAL)==0){  //  01.01.00.xx = MOD GENERAL
              // Dejo el byte como esta
              if (strcmp(argv[3],REPLACE)==0){  //  01.01.00.00 = MOD GENERAL REPLACE
                  // Dejo el byte como esta
                  parameters[*size]=argv[4];
                  (*size)++;
                  parameters[*size]=argv[5];
                  (*size)++;
              }
              else{
                printf("Error. Not a valid command for general transformations.\n");
                return -1;
              }
          }
          else if ((strcmp(argv[2],MIME)==0)){  //  01.01.01.xx = MOD MIME
              byte=byte|0x04;
              if (strcmp(argv[3],BAN)==0){  //  01.01.01.00 = MOD MIME BAN
                  // Dejo el byte como esta
                  argc=argc-4;
                  for(int j=0; j<argc; j++){
                    parameters[*size]=argv[j+4];
                    (*size)++;
                  }
              }
              else{
                printf("Error. Not a valid command for MIME transformations.\n");
                return -1;
              }
          }
          else{
              printf("Error. Not a valid command for mod.\n");
              return -1;  // OJO CON EL VALOR DE ERROR
          }
      }
  }

  return byte;
}

char metrics_encoder(char** argv, int argc, char byte, char* parameters[], int* size){
  if(strcmp(argv[1],BYTES)==0){
    // Dejo el byte como esta --> 10.00.00.00
  }
  else if(strcmp(argv[1], ACCESS)==0){
      byte=byte|0x90;
  }
  else if(strcmp(argv[1], CONECTIONS)==0){
      byte=byte|0xA0;
  }
  else{
      printf("Error. Not a valid command for metrics.\n");
      return -1;
  }
  return byte;
}

char config_encoder(char** argv, int argc, char byte, char* parameters[], int* size){
  return byte;
}
