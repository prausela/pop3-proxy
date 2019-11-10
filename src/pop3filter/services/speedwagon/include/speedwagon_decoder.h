#ifndef SPEEDWAGON_DECODER_wL7YxN65ZHqKGvCPrNbPtMJgL8B
#define SPEEDWAGON_DECODER_wL7YxN65ZHqKGvCPrNbPtMJgL8B

#define SUCCESS 	0
#define MAX_BUFFER	1024
#include <stdio.h>

char decode_request(char ebyte, char** parameters);
char sock_decoder(char ebyte, char ** parameters);
char trans_decoder(char ebyte, char ** parameters);
char metrics_decoder(char ebyte, char ** parameters);
char config_decoder(char ebyte, char ** parameters);
int show_byte(char to_decode);
char fill_byte(char value);

#endif