all:
	gcc -Wall -o pop3filter main.c services/pop3/pop3.c services/speedwagon/speedwagon_decoder.c services/selector.c services/stm.c utils/netutils.c services/buffer.c services/parser_factory.c services/pop3/nio/pop3_client_nio.c utils/lib.c services/utils/parser_utils.c services/pop3/pop3_suscriptor.c services/pop3/parsers/pop3_command_parser.c services/pop3/parsers/pop3_singleline_response_parser.c services/pop3/parsers/pop3_multiline_response_parser.c -lpthread -g

clean:
	$(RM) pop3filter