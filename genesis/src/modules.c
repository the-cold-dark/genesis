/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules.c
// ---
//
*/

#define _modules_

#include "config.h"
#include "defs.h"
#include "cdc_types.h"
#include "modules.h"
#include "moddef.h"

int init_modules(int argc, char ** argv) {
    int m;

    for (m=0; cold_modules[m] != NULL; m++)
        cold_modules[m]->init(argc, argv);

    return 1;
}

int uninit_modules(void) {
    int m;

    for (m=0; cold_modules[m] != NULL; m++)
        cold_modules[m]->uninit();

    return 1;
}

#undef _modules_
