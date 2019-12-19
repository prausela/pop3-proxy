#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "parser_utils.h"
#include "pop3_multi.h"
#include "mime_chars.h"
#include "mime_msg.h"
#include "content_type_parser.h"
#include "stack.h"
#include "list.h"
#include "parser.h"
/**
 * 
 */

struct type_list
{
    /** Media type name */
    char* type;
    /** Status to check if the string matched the parser */
    struct parser_event* event;
    /* Parser that checks if the name matched the input*/
    struct parser* type_parser;
    /** lista de eventos: si es diferente de null ocurrieron varios eventos */
    struct type_list *next;
    struct subtype_list *subtypes;
};

/**
 * 
 */
struct subtype_list
{
    /** Media substype name */
    char* subtype;

    bool* is_wildcard;
    /** Status to check if the string matched the parser */
    struct parser_event* event;
    /* Parser that checks if the name matched the input*/
    struct parser* type_parser;
    /** lista de eventos: si es diferente de null ocurrieron varios eventos */
    struct subtype_list *next;
};

