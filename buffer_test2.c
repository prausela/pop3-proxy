#include "buffer.h"
#include <stdio.h>

#define MSG "I'm so fucking tired"

int main(void){
	buffer b;
	buffer_init(&b, sizeof(MSG), MSG);
	buffer_write_adv(&b, sizeof(MSG));
	while(buffer_can_read(&b)){
		printf("%d\n", buffer_read(&b));
	}
	return 0;
}