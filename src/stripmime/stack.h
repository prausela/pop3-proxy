
#ifndef __STACK_H
#define __STACK_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>


// a common function used to free malloc'd objects
typedef void (*freeFunction)(void *);

typedef bool (*listIterator)(void *);
 
typedef struct _stackNode {
  char* data;
  struct _stackNode *next;
  struct _stackNode *prev;
} stackNode;
 
typedef struct {
  int logicalLength;
  stackNode *head;
  stackNode *tail;
  freeFunction freeFn;
} stack;
 
stack* stack_new(freeFunction freeFn);
void stack_destroy(stack *stack);
 
void stack_push(stack *stack, char* string);
int  stack_size(stack *stack);
 
char* stack_pop(stack *stack);
char* stack_peek(stack *stack);
 
#endif