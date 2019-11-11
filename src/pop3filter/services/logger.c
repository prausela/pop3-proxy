#include <stdio.h>
#include <stdlib.h>
#include "include/logs.h"

/**
 * write_log receives the message and it saves it into logs.txt placed on
 * */
static void
write_log(const char* msg){
    FILE * fPtr;
    fPtr = fopen("../../logs.txt", "a");
    fputs(msg, fPtr);
    fclose(fPtr);
}
