all:
	gcc-8 -Wall -o pop3filter src/utils/lib.c src/pop3filter/main.c src/pop3filter/services/logger.c src/pop3filter/services/speedwagon/speedwagon_decoder.c src/pop3filter/services/speedwagon/include/speedwagon_decoder.h src/pop3filter/services/selector.c src/pop3filter/services/stm.c src/pop3filter/utils/netutils.c src/pop3filter/services/buffer.c src/pop3filter/services/parser_factory.c src/pop3filter/services/pop3/nio/pop3_client_nio.c src/pop3filter/services/utils/parser_utils.c src/pop3filter/services/pop3/pop3_suscriptor.c src/pop3filter/services/pop3/pop3.c src/pop3filter/services/pop3/parsers/pop3_command_parser.c src/pop3filter/services/pop3/parsers/pop3_singleline_response_parser.c src/pop3filter/services/pop3/parsers/pop3_multiline_response_parser.c -lpthread -g
	gcc-8 -Wall -o pop3adminclient src/pop3adminclient/main.c -L/usr/local/lib -lsctp

clean:
	$(RM) pop3filter pop3adminclient
