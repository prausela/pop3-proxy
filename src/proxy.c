 #include <sys/socket.h>
 #include <sys/types.h>
 #include <resolv.h>
 #include <string.h>
 #include <stdlib.h>
 #include <pthread.h>
 #include <unistd.h>
 #include <netdb.h> //hostent
 #include <arpa/inet.h>

#define INVALID -1
 #define REQUIRED 1
 #define NOT_REQUIRED 2

 int hostname_to_ip(char * , char *);
 int is_valid_ip_address(char *);
 int checkArg(char* argument, int* expecting_data);
 // A structure to maintain client fd, and server ip address and port address
 // client will establish connection to server using given IP and port
 struct serverInfo
 {
      int client_fd;
      char ip[100];
      char port[100];
 };
 // A thread function
 // A thread for each client request


// Notas: Tiene que haber un boolean que diga si se ingreso una opcion antes.
// De ser asi, podemos procesar si esta bien o no el dato que ingresaron despues.
// Tambien podria haber otro boolean para decirnos si esa opcion requiere o no de un dato.
// Puede haber una funcion FILL en la cual le pases el dato y la variable donde guardarlo.

 void *runSocket(void *vargp)
 {
   struct serverInfo *info = (struct serverInfo *)vargp;
   char buffer[65535];
   int bytes =0;
      printf("client:%d\n",info->client_fd);
      fputs(info->ip,stdout);
      fputs(info->port,stdout);
      //code to connect to main server via this proxy server
      int server_fd =0;
      struct sockaddr_in server_sd;
      // create a socket
      server_fd = socket(AF_INET, SOCK_STREAM, 0);
      if(server_fd < 0)
      {
           printf("server socket not created\n");
      }
      printf("server socket created\n");
      memset(&server_sd, 0, sizeof(server_sd));
      // set socket variables
      server_sd.sin_family = AF_INET;
      server_sd.sin_port = htons(atoi(info->port));
      server_sd.sin_addr.s_addr = inet_addr(info->ip);
      //connect to main server from this proxy server
      if((connect(server_fd, (struct sockaddr *)&server_sd, sizeof(server_sd)))<0)
      {
           printf("server connection not established");
      }
      printf("server socket connected\n");
      while(1)
      {
        //recieve response from server
        memset(&buffer, '\0', sizeof(buffer));
        bytes = read(server_fd, buffer, sizeof(buffer));
        if(bytes <= 0){
        }
        else
        {
             // send response back to client
             write(info->client_fd, buffer, bytes);
             printf("From server :\n");
             fputs(buffer,stdout);
        }
           //receive data from client
           memset(&buffer, '\0', sizeof(buffer));
           bytes = read(info->client_fd, buffer, sizeof(buffer));
           if(bytes <= 0){
           }
           else
           {
                buffer[bytes] = '\r';
                buffer[bytes+1] = '\n';
                printf("A VER %s\n",buffer);
                // send data to main server
                write(server_fd, buffer, bytes+2);
                //printf("client fd is : %d\n",c_fd);
                printf("From client :\n");
                fputs(buffer,stdout);
                fflush(stdout);
           }

      };
   return NULL;
 }

int checkArg(char* argument, int* expecting_data){
  if(argument[0]=='-'){
    switch(argument[1]){
      case 'e':{
        *expecting_data=REQUIRED;
        break;
      }
      case 'h':{
        *expecting_data= NOT_REQUIRED;
        break;
      }
      case 'l':{
        *expecting_data= REQUIRED;
        break;
      }
      case 'L':{
        *expecting_data= REQUIRED;
        break;
      }
      case 'm':{
        *expecting_data= REQUIRED;
        break;
      }
      case 'M':{
        *expecting_data= REQUIRED;
        break;
      }
      case 'o':{
        *expecting_data= REQUIRED;
        break;
      }
      case 'p':{
        *expecting_data= REQUIRED;
        break;
      }
      case 'P':{
        *expecting_data= REQUIRED;
        break;
      }
      case 't':{
        *expecting_data= REQUIRED;
        break;
      }
      case 'v':{
        *expecting_data= NOT_REQUIRED;
        break;
      }
      default:{
        return -1;
      }
    }
    return 1;
  }
  else{
    return 0;
  }
}

 // main entry point
 int main(int argc,char *argv[])
 {
       pthread_t tid;
       char port[100],ip[100];
       char *hostname= calloc(15, sizeof(char));
       char proxy_port[100];

       // Set default values for Server Port and proxy Port
       strcpy(port,"110");  // server port
       strcpy(proxy_port,"5000"); // proxy port
       if(argc<=1)
       {
         printf("Error creating the proxy, please add a valid ip/dns, a port to connect on the origin server and a local port\n");
         return 1;
       }
        // accept arguments from terminal
        if(is_valid_ip_address(argv[1])){
          strcpy(ip,argv[1]); // server ip
        }else{
          hostname_to_ip(argv[1],hostname);
          strcpy(ip,hostname);
        }

      ////////// HAGALE PUES
      char* options[10];  // Creo array donde se guardan las opciones ingresadas por el cliente
      char* data[10];  // Creo array de punteros a char con los datos, donde la posicion de cada
                                              // uno corresponde a la posicion de las opciones.

      if(argc>2){
        int expecting_data=0;
        int is_valid;
        for(int i=2, j=0; i<argc; i++){ // Recorro cada argumento
            is_valid=checkArg(argv[i], &expecting_data);  // checkArg me devuelve 1 si es un dato, y 0 si es una opcion (por ejemplo -e)
            if (is_valid){
              options[j]=calloc(1, sizeof(char*));
              options[j]=argv[i];
              if (expecting_data==REQUIRED){
                data[j]=calloc(1, sizeof(char*));
                data[j]=argv[i+1];
              }
              else if (expecting_data==NOT_REQUIRED){
              }
              j++;
            }
            else if (is_valid==INVALID){
              printf("Invalid argument type\n");
              return 1;
            }
            else{
              //printf("%s\n",argv[i]);
            }

        }
      }

      printf("Testing database and processing your options.......\n" );
      for (int i=0; i<argc-2;i++){
        //printf("%s\t%s\n",options[i], data[i]);
        if(options[i]!=NULL){
          switch(options[i][1]){
            case 'e':{
              printf("Esta funcion esta siendo desarrollada.\n");
              break;
            }
            case 'h':{
              printf("OPCIONES:\n -e  archivo-de-error\n\tEspecifica  el  archivo  donde se redirecciona stderr de las ejecuciones de los filtros, por defecto el archivo es /dev/null.\n -h  Imprime la ayuda y termina\n -l  Direccion-pop3\n\tEstablece la direccion donde servirAi el proxy. Por defecto escucha en todas las interfaces.\n -L  Direccion de management\n\tEstablece la direccion donde donde servira el servicio de management. Por defecto escucha agonicamente en loopback.\n -p  Puerto-local\n\tPuerto TCP donde donde escuchara por conexiones entrantes pop3. Por defecto el valor es 1110\n -P  Puerto-origen Puerto TCP donde se encuentra el servidor POP3 en el servidor origen. Por defecto el valor es 110.\n -t cmd\n\t Comando utilizado para las transformaciones externas. Compatible con system(3). La seccion filtros describe como es la interaccion entre pop3filter y el comando filtro. Por defecto no se aplica ninguna transformacion.\n -v\n\tImprime informacion sobre la version y termina");
              break;
            }
            case 'l':{
              break;
            }
            case 'L':{
              printf("Esta funcion esta siendo desarrollada.\n");
              break;
            }
            case 'm':{
              printf("Esta funcion esta siendo desarrollada.\n");
              break;
            }
            case 'M':{
              printf("Esta funcion esta siendo desarrollada.\n");
              break;
            }
            case 'o':{
              printf("Esta funcion esta siendo desarrollada.\n");
              break;
            }
            case 'p':{
              strcpy(proxy_port, data[i]);
              break;
            }
            case 'P':{
              strcpy(port, data[i]);
              break;
            }
            case 't':{
              printf("Esta funcion esta siendo desarrollada.\n");
              break;
            }
            case 'v':{
              printf("Version 1.0.0\n");
              return 1;
              break;
            }
            default: {

            }
          }
        }

      }

      // A PARTIR DE ACA COMIENZO A REDIRIGIR datos

        //strcpy(port,argv[2]);  // server port
        //strcpy(proxy_port,argv[3]); // proxy port
        //hostname_to_ip(hostname , ip);
        printf("server IP : %s and port %s " , ip,port);
        printf("proxy port is %s",proxy_port);
        printf("\n");
      //socket variables
      int proxy_fd =0, client_fd=0;
      struct sockaddr_in proxy_sd;
     // add this line only if server exits when client exits
     signal(SIGPIPE,SIG_IGN);
      // create a socket
      if((proxy_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
          printf("\nFailed to create socket");
      }
      printf("Proxy created\n");
      memset(&proxy_sd, 0, sizeof(proxy_sd));
      // set socket variables
      proxy_sd.sin_family = AF_INET;
      proxy_sd.sin_port = htons(atoi(proxy_port));
      proxy_sd.sin_addr.s_addr = INADDR_ANY;
      // bind the socket
      if((bind(proxy_fd, (struct sockaddr*)&proxy_sd,sizeof(proxy_sd))) < 0)
      {
           printf("Failed to bind a socket");
      }
      // start listening to the port for new connections
      if((listen(proxy_fd, SOMAXCONN)) < 0)
      {
           printf("Failed to listen");
      }
      printf("waiting for connection..\n");
      //accept all client connections continuously
      while(1)
      {
           client_fd = accept(proxy_fd, (struct sockaddr*)NULL ,NULL);
           printf("client no. %d connected\n",client_fd);
           if(client_fd > 0)
           {
                 //multithreading variables
                 struct serverInfo *item = malloc(sizeof(struct serverInfo));
                 item->client_fd = client_fd;
                 strcpy(item->ip,ip);
                 strcpy(item->port,port);
                 pthread_create(&tid, NULL, runSocket, (void *)item);
                 sleep(1);
           }
      }
      return 0;
 }
