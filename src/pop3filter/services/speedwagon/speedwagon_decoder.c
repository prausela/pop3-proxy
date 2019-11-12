#include "include/speedwagon_decoder.h"
#include <netinet/in.h>

int total_bytes_transfered=0;

int show_byte(char to_decode){
    char a = to_decode;
    int i;
    for (i = 0; i < 8; i++) {
      printf("%d", !!((a << i) & 0x80));
    }
    printf("\n");
    return 1;
}

char decode_request(char ebyte, char ** parameters, int size){
    char byte, response;
    byte=ebyte&(0xC0);
    if(byte==0x00){ // 00.xx.xx.xx = SOCK
      printf("Sockets/");
      response=sock_decoder(ebyte, parameters);
    }
    else if(byte==0x40){ // 01.xx.xx.xx = TRANS
      printf("Transformation/");
      response=trans_decoder(ebyte, parameters, size);
    }
    else if((unsigned char)byte==(unsigned char)0x80){
      printf("Metrics/");
      response=metrics_decoder(ebyte, parameters);
    }
    else {
      printf("Configuration/");
      config_decoder(ebyte, parameters);
    }
    return response;
}

char sock_decoder(char ebyte, char ** parameters){    // Devuelve byte a responder
  char byte=ebyte;
  char response=0x00;;
    if( (byte=ebyte&(0x30))==0x00){ // 00.00.xx.xx = STATUS
      printf("Status\n");

      response=0x80;
      if((byte=ebyte&0x0C)==0x00){  // 00.00.00.xx = ORIGIN
        printf("Origin\n");
        // Function();
      }
      else if(byte==0x04){  // 00.00.01.xx = LOCAL
        printf("Local\n");
        // Function();
      }
      else{
        printf("Not implemented.\n");
        response=0x80;
        return response;
      }

    }

    else if(byte==0x10){ // 00.01.xx.xx = MOD
      printf("Modify/");

      if((byte=ebyte&0x0C)==0x00){  // 00.00.00.xx = ORIGIN
        printf("Origin/");

        if((byte=ebyte&0x01)==0x00){   // 00.00.00.00 = PORT
          printf("Port\n");
          if((parameters[0][0]>'9')||(parameters[0][0]<'0')){
            response=0x00;
          }
          else{
            response=0x80;
            // Modifico puerto
            printf("Connecting proxy to new origin server port %s", parameters[0]);
          }

        }
        else if(byte==0x01){  // 00.00.00.01 = ADDRESS
          printf("Address\n");
          printf("Connecting proxy to new origin server address \n");
        }
        else{
          printf("Not implemented. \n");
          response=0x80;
          return response;
        }

      }
      else if(byte==0x04){  // 00.00.01.xx = LOCAL
        printf("Local/");

        if((byte=ebyte&0x01)==0x00){   // 00.00.00.00 = PORT
          printf("Port\n");
        }
        else if(byte==0x01){  // 00.00.00.01 = ADDRESS
          printf("Address\n");
          return fill_byte(SUCCESS);
        }
        else{
          printf("Not implemented. \n");
          response=0x80;
          return response;
        }
      }
      else{
        printf("Not implemented.\n");
        response=0x80;
        return response;
      }

    }

    else {
      printf("Not implemented. \n");
      response=0x80;
      return response;
    }

    return response;
}

char trans_decoder(char ebyte, char ** parameters, int size){
  char byte=ebyte;
  char response=0x00;
    if(ebyte==0x40){ // 01.00.xx.xx = STATUS
      printf("Status\n");
      printf("Currently filtering the following media types: ");
      char * aux = getenv("FILTER_MEDIAS");
      if(aux!=NULL){
        printf("%s\n",aux );
      }
      else{
        printf("%s\n");
      }
      response=0x80;
    }

    else if((byte&0x50)==0x50){ // 01.01.xx.xx = MOD
      printf("Modify/");

      if((byte=ebyte&0x5C)==0x50){  // 01.01.00.xx = GENERAL
        printf("General/");

        if(ebyte==0x50){   // 01.01.00.00 = REPLACE
          printf("Replace\n");
        }
        else{
          printf("Not implemented. \n");
          response=0x80;
          return response;
        }

      }

      else if((byte=ebyte&0x54)==0x54){  // 01.01.01.xx = MIME
        printf("Mime/");

        if(ebyte==0x54){   // 01.01.01.00 = BAN
          printf("ban\n");
          char * types=NULL;
          int word_size=0, pointer=0;
          total_bytes_transfered+=2;

          for(int i=0; i<size; i++){
            word_size+=(strlen(parameters[i]));
            //printf("Tamaño de la palabra: %s es %d\n",parameters[i], word_size);
            //printf("Tamaño de bytes reservados: %d\n",word_size+1 );
            types = realloc(types, word_size+1);
          }
          total_bytes_transfered+=word_size;

          for(int i=0; i<size; i++){
            strcpy(types+pointer, parameters[i]);
            //printf("Copiando palabra: %s con el valor de puntero: %d\n",parameters[i], pointer);
            pointer+=strlen(parameters[i])+1;
            if((i+1)>=size){  // Si es el ultimo parametro...
              types[pointer-1]=0;
            }
            else{
              types[pointer-1]=',';
            }
          }

          setenv("FILTER_MSG", " !____MENSAJE DE REEMPLAZO_____! ", 1);
          setenv("FILTER_MEDIAS",types, 1);
          char * aux = getenv("FILTER_MEDIAS");
          printf("Current filtered MIME types are: %s\n",aux );

          response=0x80;
        }
        else{
          printf("Not implemented. \n");
          response=0x80;
          return response;
        }
      }

      else{
        printf("Not implemented.\n");
        response=0x80;
        return response;
      }

    }

    else {
      printf("Not implemented. \n");
      response=0x80;
      return response;
    }

    return response;
}

char metrics_decoder(char ebyte, char ** parameters){
  char byte=ebyte;
  char response=0x00;
  //printf("EL byte ingresado es: ");
  //show_byte(byte);
  total_bytes_transfered+=2;
  if( ebyte==(char)0x80){
    printf("Total bytes\n");
    printf("%d total of bytes transfered\n", total_bytes_transfered);
    response=0x80;
  }
  else if(ebyte==(char)0x90){
    printf("Access log\n");
    response=0x80;
  }
  else if(ebyte==(char)0xA0){
    printf("Concurrent connections\n");
    response=0x80;
  }
  else{

  }

  return response;
}

char config_decoder(char ebyte, char ** parameters){
  char byte=ebyte;
  char response=0x80;



  return response;
}

char fill_byte(char value){
  char ret;
  if(value==(char)SUCCESS){
    ret=0x00;
  }
  else{
    ret=0x80;
  }
  return ret;
}
