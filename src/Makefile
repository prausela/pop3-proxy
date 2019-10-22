all: proxy.c
	gcc -g -Wall -o proxy proxy.c ./utils/lib.c ./services/transformations.c ./services/dns_lookup.c ./services/validate_ip.c parser_factory.c parser_utils.c pop3_singleline_response_parser.c pop3_multiline_response_parser.c pop3_parser.c pop3_command_parser.c -lpthread
	gcc -o pop3_command_parser parser_factory.c parser_utils.c pop3_command_parser.c testing/pop3_command_parser_test.c
	gcc -o pop3_singleline_response_parser parser_factory.c parser_utils.c pop3_singleline_response_parser.c testing/pop3_singleline_response_parser_test.c -g
	gcc -o pop3_multiline_response_parser parser_factory.c testing/pop3_multiline_response_parser_test.c pop3_multiline_response_parser.c parser_utils.c -g
	gcc -o pop3_parser parser_factory.c parser_utils.c pop3_singleline_response_parser.c pop3_multiline_response_parser.c pop3_parser.c testing/pop3_parser_test.c pop3_command_parser.c -g
	gcc -Wall -o testing_transformations testing/transformation_testing.c services/transformations.c

clean:
	$(RM) proxy
	$(RM) pop3_command_parser
	$(RM) pop3_singleline_response_parser
	$(RM) pop3_multiline_response_parser
	$(RM) pop3_parser
	$(RM) testing_transformations
