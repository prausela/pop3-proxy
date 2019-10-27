#ifndef SERVICES_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM
#define SERVICES_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM

int is_valid_ip_address(char *ipAddress);
int hostname_to_ip(char * hostname, char *ip);
int create_transformation(int * sender_pipe, int * receiver_pipe);
int encrypt(char *source , char *encrypted_string);
int decrypt(char *source , char *encrypted_string);
#endif
