#include "include/stack.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct stack_node{
	char *data;
	ssize_t data_len;
	struct stack_node * next;
} stack_node_t;

typedef stack_node_t *StackNode;

typedef struct stack {
	stack_node_t *top;
	stack_node_t *last;
} stack_t;


Stack init_stack(void){
	Stack stack = malloc(sizeof(stack_t));
	stack->top = NULL;
	return stack;
}

void push(Stack stack, char* data, ssize_t data_len){
	StackNode node = malloc(sizeof(stack_node_t));
	node->data = malloc(data_len);
	node->data_len = data_len;
	strncpy(node->data, data, data_len);
	if(stack->top != NULL){
		node->next = NULL;
		stack->top = node;
	} else {
		node->next = stack->top;
		stack->top = node;
	}
}

void pop(Stack stack, char* data, ssize_t *data_len){
	strncpy(data, stack->top->data, stack->top->data_len);
	*data_len = stack->top->data_len;
	free(stack->top->data);
	StackNode aux = stack->top->next;
	free(stack->top);
	stack->top = aux;
}

void free_stack(Stack stack){
	free(stack);
}