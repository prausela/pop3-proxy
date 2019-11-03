#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * 
 */
struct type_list
{
    /** Media type name */
    char* type;
    /** Flag to check if types is a wildcard */
    bool* is_wildcard;
    /** Status to check if the string matched the parser */
    uint8_t status;
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
    char* type;
    /** Status to check if the string matched the parser */
    uint8_t status;
    /* Parser that checks if the name matched the input*/
    struct parser* type_parser;
    /** lista de eventos: si es diferente de null ocurrieron varios eventos */
    struct subtype_list *next;
};