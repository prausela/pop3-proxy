#ifndef STACK_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM
#define STACK_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM

#include <sys/types.h>

typedef struct stack *Stack;

Stack init_stack(void);

void push(Stack stack, char* data, ssize_t data_len);

void pop(Stack stack, char* data, ssize_t *data_len);

void free_stack(Stack stack);

#endif