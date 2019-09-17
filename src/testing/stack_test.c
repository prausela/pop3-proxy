#include "../include/stack.h"

#define HOLA "hola"

int main(void){
	char buff[256];
	int h;
	Stack stack = init_stack();
	push(stack, HOLA, 4);
	pop(stack, buff, &h);
	printf("%s\n", buff);
	free_stack(stack);
	return 0;
}