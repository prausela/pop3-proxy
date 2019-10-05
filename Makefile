all:
	gcc -o proxy main.c selector.c socks5.c socks5nio.c stm.c request.c netutils.c buffer.c hello.c socks5suscriptor.c -lpthread

clean:
	$(RM) proxy