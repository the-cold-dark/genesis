/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_network.c
// ---
// Network Module
*/

#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "cdc_types.h"
#include "execute.h"
#include "net.h"

/*
// -----------------------------------------------------------------
//
// If the current object has a connection, it will reassign that
// connection too the specified object.
//
*/

/*
// -----------------------------------------------------------------
*/
void op_hostname(void) {
    data_t *args;
    string_t *r;

    /* Accept a port number. */
    if (!func_init_1(&args, STRING))
        return;

    r = hostname(args[0].u.str->s);

    pop(1);
    push_string(r);
}

/*
// -----------------------------------------------------------------
*/
void op_ip(void) {
    data_t *args;
    string_t *r;

    /* Accept a hostname. */
    if (!func_init_1(&args, STRING))
        return;

    r = ip(args[0].u.str->s);

    pop(1);
    push_string(r);
}
