/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_integer.c
// ---
// Miscellaneous operations.
*/

#include "config.h"
#include "defs.h"

#include "y.tab.h"
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "util.h"

INTERNAL void find_extreme(int which);

void op_random(void) {
    data_t *args;

    /* Take one integer argument. */
    if (!func_init_1(&args, INTEGER))
	return;

    /* Replace argument on stack with a random number. */
    args[0].u.val = random_number(args[0].u.val) + 1;
}

/* which is 1 for max, -1 for min. */
INTERNAL void find_extreme(int which) {
    int arg_start, num_args, i, type;
    data_t *args, *extreme, d;

    arg_start = arg_starts[--arg_pos];
    args = &stack[arg_start];
    num_args = stack_pos - arg_start;

    if (!num_args) {
	cthrow(numargs_id, "Called with no arguments, requires at least one.");
	return;
    }

    type = args[0].type;
    if (type != INTEGER && type != STRING && type != FLOAT) {
	cthrow(type_id, "First argument (%D) not an integer, float or string.",
	      &args[0]);
	return;
    }

    extreme = &args[0];
    for (i = 1; i < num_args; i++) {
	if (args[i].type != type) {
	    cthrow(type_id, "Arguments are not all of same type.");
	    return;
	}
	if (data_cmp(&args[i], extreme) * which > 0)
	    extreme = &args[i];
    }

    /* Replace args[0] with extreme, and pop other arguments. */
    data_dup(&d, extreme);
    data_discard(&args[0]);
    args[0] = d;
    pop(num_args - 1);
}

void op_max(void) {
    find_extreme(1);
}

void op_min(void) {
    find_extreme(-1);
}

void op_abs(void) {
    data_t *args;

    if (!func_init_1(&args, INTEGER))
	return;

    if (args[0].u.val < 0)
	args[0].u.val = -args[0].u.val;
}

