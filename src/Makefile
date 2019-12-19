CC=gcc
CFLAGS=-Wall -Wextra
all:
	$(CC) $(CFLAGS) -o ../stripmime stripmime/mime_chars.c stripmime/mime_msg.c stripmime/parser_utils.c stripmime/parser.c stripmime/pop3_multi.c stripmime/stripmime.c stripmime/content_type_parser.c stripmime/list.c stripmime/stack.c
	$(CC) $(CFLAGS) -o ../pop3filter pop3filter/utils/lib.c pop3filter/main.c pop3filter/services/logger.c pop3filter/services/speedwagon/speedwagon_decoder.c pop3filter/services/selector.c pop3filter/services/stm.c pop3filter/utils/netutils.c pop3filter/services/buffer.c pop3filter/services/parser_factory.c pop3filter/services/pop3/nio/pop3_client_nio.c pop3filter/services/pop3/nio/pop3_admin_nio.c pop3filter/services/utils/parser_utils.c pop3filter/services/pop3/pop3_suscriptor.c pop3filter/services/pop3/pop3.c pop3filter/services/pop3/parsers/pop3_command_parser.c pop3filter/services/pop3/parsers/pop3_singleline_response_parser.c pop3filter/services/pop3/parsers/pop3_multiline_response_parser.c pop3filter/services/transformations.c -lpthread -g
	$(CC) $(CFLAGS) -o ../pop3ctl pop3ctl/main.c -L/usr/local/lib -lsctp

clean:
	$(RM) ../stripmime
	$(RM) ../pop3filter
	$(RM) ../pop3ctl
