/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_object.c
// ---
// Miscellaneous operations.
*/

#include "config.h"
#include <stdlib.h>
#include "defs.h"
#include "y.tab.h"
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"
#include "util.h"
#include "lookup.h"

void op_get_dbref(void) {
    data_t *args;
    long dbref;

    if (!func_init_1(&args, SYMBOL))
	return;

    if (!lookup_retrieve_name(args[0].u.symbol, &dbref)) {
	cthrow(namenf_id, "Cannot find object %I.", args[0].u.symbol);
	return;
    }

    pop(1);
    push_dbref(dbref);
}

