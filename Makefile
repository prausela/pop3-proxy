all:
	gcc -Wall -o pop3filter main.c services/speedwagon/speedwagon_decoder.c services/selector.c services/stm.c utils/netutils.c services/buffer.c services/parser_factory.c services/pop3/nio/pop3_admin_nio.c services/utils/parser_utils.c services/pop3/pop3_suscriptor.c -lpthread -g

clean:
	$(RM) pop3filter