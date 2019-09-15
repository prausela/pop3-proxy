all: proxy.c
	gcc -g -Wall -o proxy proxy.c ./services/dns_lookup.c ./services/validate_ip.c -lpthread
	gcc -o parser parser_factory.c parser_utils.c pop3_command_parser.c pop3_command_parser_test.c -g
	gcc -Wall -o testing_transformations testing/transformation_testing.c services/transformations.c

clean:
	$(RM) proxy
	$(RM) parser
