/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/moduledef.h
// ---
//
*/

#ifndef _moduledef_h_
#define _moduledef_h_

#include "modules.h"

module_t coldcore_module = {init_coldcore, uninit_coldcore};

module_t *cold_modules[] = {
    &coldcore_module,
    NULL
};

#endif
