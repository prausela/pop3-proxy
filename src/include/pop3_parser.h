#ifndef POP3_PARSER_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM
#define POP3_PARSER_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>


struct parser *
pop3_parser_init(void);

const struct parser_event *
pop3_parser_feed(struct parser *p, const uint8_t c);

#endif