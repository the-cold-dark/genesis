/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/decode.h
// ---
// Declarations for decompiling methods.
*/

#ifndef _decode_h_
#define _decode_h_

#include "data.h"
#include "object.h"

int      line_number(method_t * method, int pc);

list_t * decompile(method_t * method,
                   object_t * object,
                   int        increment,
                   int        parens);

#endif

