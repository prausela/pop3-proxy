#include "include/parser_utils.h"

/**
 *	enum parser_utils_event_type {
 *		IGNORE 		= 0,
 *		TRAPPED 	= 1,
 *	};
 */

/** This event's intention of purpose is to IGNORE the input given.
 *
 *		~~EVENT~~
 *      type 	🡢 IGNORE
 *		n 		🡢 0
 *		data 	🡢 ~
 */

void
ignore(struct parser_event *ret, const uint8_t c){
	ret->type 		= IGNORE;
	ret->n 			= 0;
}

/** This event's intention of purpose is to TRAP the input given.
 * 	This means that the chain is not accepted by the automata.
 *
 *		~~EVENT~~
 *      type 	🡢 TRAPPED
 *		n 		🡢 0
 *		data 	🡢 ~
 */

void
trap(struct parser_event *ret, const uint8_t c){
	ret->type 		= TRAPPED;
	ret->n 			= 0;
}

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

const struct parser_state_transition ST_TRAP [] = {
	{.when = ANY, 		.dest = TRAP, 		.act1 = trap, 	},
};