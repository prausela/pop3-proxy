#include "include/speedwagon_decoder.h"
#include <netinet/in.h>

int show_byte(char to_decode){
    char a = to_decode;
    int i;
    for (i = 0; i < 8; i++) {
      printf("%d", !!((a << i) & 0x80));
    }
    printf("\n");
    return 1;
}

char decode_request(char ebyte, char ** parameters){
    char byte, response;
    byte=ebyte&(0xC0);
    if(byte==0x00){ // 00.xx.xx.xx = SOCK
      printf("ENTRO A SOCK\n");
      response=sock_decoder(ebyte, parameters);
    }
    else if(byte==0x40){ // 01.xx.xx.xx = TRANS
      printf("ENTRO A TRANS\n");
      response=trans_decoder(ebyte, parameters);
    }
    else if((unsigned char)byte==(unsigned char)0x80){
      printf("ENTRO A METRICS\n");
      metrics_decoder(ebyte, parameters);
    }
    else {
      printf("ENTRO A CONF\n");
      config_decoder(ebyte, parameters);
    }
    return response;
}

char sock_decoder(char ebyte, char ** parameters){    // Devuelve byte a responder
  char byte=ebyte;
  char response=0x00;;
    if( (byte=ebyte&(0x30))==0x00){ // 00.00.xx.xx = STATUS
      printf("ENTRO A STATUS\n");

      if((byte=ebyte&0x0C)==0x00){  // 00.00.00.xx = ORIGIN
        printf("ENTRO A ORIGIN\n");
        // Function();
      }
      else if(byte==0x04){  // 00.00.01.xx = LOCAL
        printf("ENTRO A LOCAL\n");
        // Function();
      }
      else{
        printf("Not implemented.\n");
        response=0x80;
        return response;
      }

    }

    else if(byte==0x10){ // 00.01.xx.xx = MOD
      printf("ENTRO A MOD\n");

      if((byte=ebyte&0x0C)==0x00){  // 00.00.00.xx = ORIGIN
        printf("ENTRO A ORIGIN\n");

        if((byte=ebyte&0x01)==0x00){   // 00.00.00.00 = PORT
          printf("ENTRO A PORT\n");
        }
        else if(byte==0x01){  // 00.00.00.01 = ADDRESS
          printf("ENTRO A ADDRESS\n");
        }
        else{
          printf("Not implemented. \n");
          response=0x80;
          return response;
        }

      }
      else if(byte==0x04){  // 00.00.01.xx = LOCAL
        printf("ENTRO A LOCAL\n");

        if((byte=ebyte&0x01)==0x00){   // 00.00.00.00 = PORT
          printf("ENTRO A PORT\n");
        }
        else if(byte==0x01){  // 00.00.00.01 = ADDRESS
          printf("ENTRO A ADDRESS\n");
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

char trans_decoder(char ebyte, char ** parameters){
  char byte=ebyte;
  char response=0x00;
    if(ebyte==0x40){ // 01.00.xx.xx = STATUS
      printf("ENTRO A STATUS\n");
    }

    else if((byte=ebyte|0x50)==0x50){ // 01.01.xx.xx = MOD
      printf("ENTRO A MOD\n");

      if((byte=ebyte&0x5C)==0x50){  // 01.01.00.xx = GENERAL
        printf("ENTRO A GENERAL\n");

        if(ebyte==0x50){   // 01.01.00.00 = REPLACE
          printf("ENTRO A REPLACE\n");
        }
        else{
          printf("Not implemented. \n");
          response=0x80;
          return response;
        }

      }

      else if((byte=ebyte&0x54)==0x54){  // 01.01.01.xx = MIME
        printf("ENTRO A MIME\n");

        if(ebyte==0x54){   // 01.01.01.00 = BAN
          printf("ENTRO A BAN\n");
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



  return response;
}

char config_decoder(char ebyte, char ** parameters){
  char byte=ebyte;
  char response=0x00;



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
