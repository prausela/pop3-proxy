

/** 
 *   +---------------------------------------------------------------------------+
 *   |+-------------------------------------------------------------------------+|
 *   ||                            ~~IMPORTANT!!~~                              ||
 *   ||		If this utilities library is included:								||
 *   ||		             1) parser_event_type 	enums MUST BEGIN AT 2. 			||
 *   ||                  2) parser_states 		enums MUST BEGIN AT 1.			||
 *   ||																			||
 *   |+-------------------------------------------------------------------------+|
 *   +---------------------------------------------------------------------------+
 */


#ifndef PARSER_UTILS_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM
#define PARSER_UTILS_H_ACK0Q8X1WrbMI9ZFlPxsr98iOM

#include "parser_factory.h"

	enum parser_utils_states {
		TRAP 	= 0,
	};

	enum parser_utils_event_type {
		IGNORE 		= 0,
		TRAPPED 	= 1,
	};

/** This event's intention of purpose is to IGNORE the input given.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 */

void
ignore(struct parser_event *ret, const uint8_t c);

/** This event's intention of purpose is to TRAP the input given.
 * 	This means that the chain is not accepted by the automata.
 *
 *		~~EVENT~~
 *      type 	🡢 TRAPPED
 *		n 		🡢 0
 *		data 	🡢 ~
 */

void
trap(struct parser_event *ret, const uint8_t c);

/** ~~TRAP STATE TRANSITIONS~~
 * One cannot transition from the TRAP STATE to any other state.
 * TRAP STATE's transitions are as follows:
 *
 *      +----+----+
 *      |TRAP|    |ANY
 *      +----+<---+
 *
 * TRAP 	ANY🡢 TRAP :
 *
 * 		If we have entered the TRAP STATE it means the chain is not accepted,
 * 		Thus it will never be accepted.
 *
 *		~~EVENT~~
 *      type 	🡢 TRAPPED
 *		n 		🡢 0
 *		data 	🡢 ~
 *
 */

const struct parser_state_transition ST_TRAP[1];

#endif