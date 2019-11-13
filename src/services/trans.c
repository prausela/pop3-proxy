#include "services.h"
#include <unistd.h>


int main(void){
	int receiver[2], sender[2];
	int res = create_transformation(sender, receiver);
	if(!res){
		write(sender[0], "Tengo suenio", sizeof("Tengo suenio"));
	}
	return 0;
}