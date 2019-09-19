#include "../include/lib.h"

/* Check if an argument is correc and if it's required for pop3          */
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
      return -1;
    }
    }
    return 1;
  }
  else
  {
    return 0;
  }
}


/* This functions parses the input from the command line           */
int command_line_parser(int argc,char **argv,char * proxy_port,char * port)
{
  //Definition of variables
  int expecting_data,i,j,is_valid;
  char *options[10]; // Array for inserted options (by client)
  char *data[10];    // Array where each position belongs to the position of the
                     // options

  //Only analize data if it has at least one argument
  if (argc > 2)
  {
    expecting_data = 0;
    for (i = 1, j = 0; i < argc - 1; i++)
    { // Iterate for each argument
      //is_valid is 1 if its data or0 if its an option
      is_valid = checkArg(argv[i], &expecting_data);
      if (is_valid == REQUIRED)
      {
        options[j] = calloc(1, sizeof(char *));
        options[j] = argv[i];
        if (expecting_data == REQUIRED)
        {
          data[j] = calloc(1, sizeof(char *));
          data[j] = argv[i + 1];
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
    }
  }

  for (int i = 0; i < argc - 2; i++)
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
        break;
      }
      case 'L':
      {
        printf("This function is being developed.\n");
        break;
      }
      case 'm':
      {
        printf("This function is being developed.\n");
        break;
      }
      case 'M':
      {
        printf("This function is being developed.\n");
        break;
      }
      case 'o':
      {
        printf("This function is being developed.\n");
        break;
      }
      case 'p':
      {
        strcpy(proxy_port, data[i]);
        break;
      }
      case 'P':
      {
        strcpy(port, data[i]);
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
