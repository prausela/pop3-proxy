all: proxy.c
	gcc -g -Wall -o proxy proxy.c ./services/dns_lookup.c ./services/validate_ip.c -lpthread
	gcc -o pop3_command_parser parser_factory.c parser_utils.c pop3_command_parser.c pop3_command_parser_test.c
	gcc -o pop3_singleline_response_parser parser_factory.c parser_utils.c pop3_singleline_response_parser.c pop3_singleline_response_parser_test.c -g


clean:
	$(RM) proxy
	$(RM) pop3_command_parser
	$(RM) pop3_singleline_response_parser
