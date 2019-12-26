#include "include/lib.h"
#include <string.h>
#include <getopt.h>

extern struct pop3filter_arguments pop3filter_arguments_default = {"/dev/null", "0.0.0.0", "localhost", 1110, 9090, 110, "cmd"};

#define strtoint(_str) strtol(_str, (char **)NULL, 10)

int options_handler(const int argc, const char **argv, struct pop3filter_arguments *pop3filter_arguments){
    int opt; 
      
    // put ':' in the starting of the 
    // string so that program can  
    //distinguish between '?' and ':'  
    while((opt = getopt(argc, argv, ":e:hl:L:m:M:o:p:P:t:v")) != -1)  
    {  
        switch(opt)  
        {
			case 'e':
				pop3filter_arguments->error_filename = strdup(optarg);
				break;
            case 'h':
            case 'l':
				pop3filter_arguments->client_address = strdup(optarg);
				break;
            case 'L':
				pop3filter_arguments->client_address = strdup(optarg);
				break;
            case 'm':
            case 'M':
            case 'o':
				pop3filter_arguments->admin_port = strtoint(optarg);
				break;
            case 'p':
            case 'P':
            case 't':
            case 'v':
            case 'i':
            case 'r':  
                printf("option: %c\n", opt);  
                break;  
            case 'f':  
                printf("filename: %s\n", optarg);  
                break;  
            case ':':  
                printf("option needs a value %c\n", optopt);  
                break;  
            case '?':  
                printf("unknown option: %c\n", optopt); 
                break;  
        }  
    }  
      
    // optind is for the extra arguments 
    // which are not parsed 
    for(; optind < argc; optind++){      
        printf("extra arguments: %s\n", argv[optind]);  
    } 
}

/* Check if an argument is correc and if it's required for pop3          
int checkArg(char *argument, int *expecting_data)
{
  if (argument[0] == '-')
  {
    switch (argument[1])
    {
    case 'e':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'h':
    {
      *expecting_data = NOT_REQUIRED;
      break;
    }
    case 'l':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'L':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'm':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'M':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'o':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'p':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'P':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 't':
    {
      *expecting_data = REQUIRED;
      break;
    }
    case 'v':
    {
      *expecting_data = NOT_REQUIRED;
      break;
    }
    default:
    {
      return INVALID;
    }
    }
    return VALID;
  }
  else
  {
    return ADDRESS;
  }
}

/* This functions parses the input from the command line           
int command_line_parser(int argc,char **argv,char* proxy_address, char* proxy_address_ipv6, char* client_port, char* admin_address, char* admin_address_ipv6, char* admin_port, char* origin_address, char* origin_port)
{

  //Definition of variables
  int expecting_data,i,j,is_valid;
  char *options[10]; // Array for inserted options (by client)
  char *data[10];    // Array where each position belongs to the position of the
                     // options

  for (int i=0; i<10; i++){
    options[i]=NULL;
  }
  //Only analize data if it has at least one argument
  if (argc >= 2)
  {
    expecting_data = 0;
    for (i = 1, j = 0; i < argc; i++)
    { // Iterate for each argument
      //is_valid is 1 if its data or 0 if its an option
      is_valid = checkArg(argv[i], &expecting_data);
      if (is_valid == VALID)
      {
        //options[j] = calloc(1, sizeof(char *));
        options[j] = argv[i];
        if (expecting_data == REQUIRED)
        {
          data[j] = calloc(1, sizeof(char *));
          data[j] = argv[++i];
        }
        else if (expecting_data == NOT_REQUIRED)
        {
        }
        j++;
      }
      else if (is_valid == INVALID)
      {
        printf("Invalid argument type\n");
        return 1;
      }
      else if(is_valid==ADDRESS){
        strcpy(origin_address, argv[i]);
      }
    }
  }

  for (int i = 0; i < argc - 1; i++)
  {
    if (options[i] != NULL)
    {
      switch (options[i][1])
      {
      case 'e':
      {
        printf("This function is being developed.\n");
        break;
      }
      case 'h':
      {
        printf("OPCIONES:\n -e  archivo-de-error\n\tEspecifica  el  archivo  donde se redirecciona stderr de las ejecuciones de los filtros, por defecto el archivo es /dev/null.\n -h  Imprime la ayuda y termina\n -l  Direccion-pop3\n\tEstablece la direccion donde servirAi el proxy. Por defecto escucha en todas las interfaces.\n -L  Direccion de management\n\tEstablece la direccion donde donde servira el servicio de management. Por defecto escucha agonicamente en loopback.\n -p  Puerto-local\n\tPuerto TCP donde donde escuchara por conexiones entrantes pop3. Por defecto el valor es 1110\n -P  Puerto-origen Puerto TCP donde se encuentra el servidor POP3 en el servidor origen. Por defecto el valor es 110.\n -t cmd\n\t Comando utilizado para las transformaciones externas. Compatible con system(3). La seccion filtros describe como es la interaccion entre pop3filter y el comando filtro. Por defecto no se aplica ninguna transformacion.\n -v\n\tImprime informacion sobre la version y termina\n");
        return 0;
      }
      case 'l':
      {
        char aux[100];
        strcpy(aux, data[i]);
        if(inet_pton(AF_INET, aux, proxy_address)){
          strcpy(proxy_address, data[i]);
        }
        else if(inet_pton(AF_INET6, aux, proxy_address_ipv6)){
          strcpy(proxy_address_ipv6, data[i]);
        }
        break;
      }
      case 'L':
      {
        char aux2[100];
        strcpy(aux2, data[i]);
        if(inet_pton(AF_INET, aux2, admin_address)){
          strcpy(admin_address, data[i]);
        }
        else if(inet_pton(AF_INET6, aux2, admin_address_ipv6)){
          strcpy(admin_address_ipv6, data[i]);
        }
        else{
          printf("No pude cambiar la direccion del proxy pq no entiendo nada. Admin\n");
        }
      }
      case 'm':
      {
        printf("This function is being developed.\n");
        break;
      }
      case 'M':
      {
        printf("Administrator address.\n");
        break;
      }
      case 'o':
      {
        strcpy(admin_port, data[i]);
        printf("Changing SCTP port.\n");
        break;
      }
      case 'p':   // client port
      {
        strcpy(client_port, data[i]);
        printf("Changing TCP port.\n");
        break;
      }
      case 'P':
      {
        strcpy(origin_port, data[i]);
        break;
      }
      case 't':
      {
        printf("This function is being developed.\n");
        break;
      }
      case 'v':
      {
        printf("Version 1.0.0\n");
        return 0;
      }
      default:
      {
      }
      }
    }
  }
  return 1;
}
*/