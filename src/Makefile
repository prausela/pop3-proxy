all: proxy.c
	gcc-8 -g -Wall -o proxy proxy.c ./utils/lib.c ./services/transformations.c ./services/dns_lookup.c ./services/validate_ip.c parser_factory.c parser_utils.c pop3_singleline_response_parser.c pop3_multiline_response_parser.c pop3_parser.c pop3_command_parser.c -lpthread
	gcc-8 -o pop3_command_parser parser_factory.c parser_utils.c pop3_command_parser.c testing/pop3_command_parser_test.c
	gcc-8 -o pop3_singleline_response_parser parser_factory.c parser_utils.c pop3_singleline_response_parser.c testing/pop3_singleline_response_parser_test.c -g
	gcc-8 -o pop3_multiline_response_parser parser_factory.c testing/pop3_multiline_response_parser_test.c pop3_multiline_response_parser.c parser_utils.c -g
	gcc-8 -o pop3_encrypt_parser parser_factory.c services/encrypt.c testing/pop3_encrypt_parser_test.c pop3_encrypt_parser.c parser_utils.c -g
	gcc-8 -o pop3_decrypt_parser parser_factory.c services/decrypt.c testing/pop3_decrypt_parser_test.c pop3_decrypt_parser.c parser_utils.c -g
	gcc-8 -o pop3_parser parser_factory.c parser_utils.c  pop3_singleline_response_parser.c pop3_multiline_response_parser.c pop3_parser.c testing/pop3_parser_test.c pop3_command_parser.c -g
	gcc-8 -Wall -o testing_transformations testing/transformation_testing.c services/transformations.c
	gcc-8 -Wall -o mime_filter ./mimi_filter/src/mime_chars.c ./mimi_filter/src/mime_msg.c ./mimi_filter/src/parser_utils.c ./mimi_filter/src/parser.c ./mimi_filter/src/pop3_multi.c ./mimi_filter/src/stripmime.c ./mimi_filter/src/content_type_parser.c ./mimi_filter/src/list.c ./mimi_filter/src/stack.c


clean:
	$(RM) proxy
	$(RM) pop3_command_parser
	$(RM) pop3_singleline_response_parser
	$(RM) pop3_multiline_response_parser
	$(RM) pop3_encrypt_parser
	$(RM) pop3_decrypt_parser
	$(RM) pop3_parser
	$(RM) testing_transformations
	$(RM) mime_filter