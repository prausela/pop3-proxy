#include "stack.h"
 

stack* stack_new(freeFunction freeFn)
{
    stack *s = malloc(sizeof(stack));
    s->logicalLength = 0;
    s->tail = NULL;
    s->freeFn = freeFn;
    s->head = NULL;
  return s;
}

void stack_destroy(stack *stack)
{
  stackNode *current;
  while(stack->head != NULL) {
    current = stack->head;
    stack->head = current->next;

    //hacer free del element
    free(current);
  }
}

void stack_push(stack *stack, char* element)
{

  stackNode *node = malloc(sizeof(stackNode));
  printf("STACK PUSH\n");
  printf("ELEM SIZE %ld\nString: %s\n",strlen(element),element);
  //node->data=element;
  node->data=malloc(strlen(element));
//   printf("NODE DATA LENGHT1: %ld\n",strlen(node->data));
  memcpy(node->data, element,strlen(element));//+1 del \0?
  node->data[strlen(element)]=0;
//   printf("NODE DATA LENGHT: %ld\n",strlen(node->data));
//   printf("NODE DATA %s\n",node->data);
  node->next = NULL;
  node->prev=NULL;
          
         
  if(stack->logicalLength == 0) {
    stack->head = stack->tail = node;

  } else {
      node->prev=stack->tail;
    stack->tail->next = node;
    stack->tail = node;
  }
    stack->logicalLength++;
}

char* stack_pop(stack *stack)
{
 if(stack->logicalLength==0){
     return NULL;
 }
    char* elem=stack->tail->data;
    
    stackNode* node=stack->tail;
    stack->tail=stack->tail->prev;
    free(node);
    stack->logicalLength--;
    return elem;
  }


int stack_size(stack *stack)
{
  return stack->logicalLength;
}

char* stack_peek(stack *stack){
    return stack->head->data;
}