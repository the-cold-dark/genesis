/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: native.c
// ---
//
// this file handles native methods
//
// adding native methods this way is inefficient, but we only do it at bootup,
// and it guarantee's that the array is only as big as we need it, as it does
// not grow
*/

#define _native_

#include "config.h"
#include "defs.h"
#include "object.h"
#include "native.h"
#include "moddef.h"
#include "memory.h"
#include "cdc_types.h"
#include "cache.h"
#include "lookup.h"
#include "grammar.h"

int init_modules(int argc, char ** argv) {
    int m;

    for (m=0; m < NUM_MODULES; m++) {
        if (cold_modules[m]->init != (void *) NULL)
            cold_modules[m]->init(argc, argv);
    }

    return 1;
}

int uninit_modules(void) {
    int m;

    for (m=0; m < NUM_MODULES; m++) {
        if (cold_modules[m]->init != (void *) NULL)
            cold_modules[m]->uninit();
    }

    return 1;
}

#undef _native_
