/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/object.h
// ---
*/

#ifndef _native_h_
#define _native_h_

#include <stdio.h>
#include "cdc_types.h"
#include "io.h"
#include "file.h"

/* this structure is used only to initialize methods */
/* we pull the name symbol and put it in the actual method definition */
/* we need num_args defined here so we can drop it into the method def */
typedef struct native_s {
    char     * name;
    char     * bindobj;
    void (*func)(void);
    int        args;
    int        rest;
} native_t;

#ifdef _modules_

native_t ** native_methods;

#else

extern native_t ** native_methods;

#endif

#endif
