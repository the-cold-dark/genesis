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
#include "moduledef.h"

Ident pabort_id, pclose_id, popen_id;

int init_modules(int argc, char ** argv) {
    int m;

    for (m=0; cold_modules[m] != NULL; m++)
        cold_modules[m]->init(argc, argv);

    return 1;
}

int uninit_modules(int argc, char ** argv) {
    int m;

    for (m=0; cold_modules[m] != NULL; m++)
        cold_modules[m]->uninit(argc, argv);

    return 1;
}

void init_coldcore(int argc, char ** argv) {
    pabort_id = ident_get("abort");
    pclose_id = ident_get("close");
    popen_id  = ident_get("open");
}

void uninit_coldcore(void) {
    ident_discard(pabort_id);
    ident_discard(pclose_id);
    ident_discard(popen_id);
}

#undef _modules_
