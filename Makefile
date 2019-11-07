all:
	gcc -o pop3filter main.c selector.c stm.c netutils.c buffer.c parser_factory.c pop3_admin_nio.c parser_utils.c pop3_suscriptor.c -lpthread -g

clean:
	$(RM) pop3filter