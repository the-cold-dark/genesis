/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/grammar.h
// ---
// Declarations for the parser.
*/

#ifndef _grammar_h_
#define _grammar_h_

#include <stdarg.h>
#include "config.h"
#include "cdc_types.h"
#include "object.h"
#include "list.h"

method_t * compile(object_t *object, list_t * code, list_t ** error_ret);
void       compiler_error(int lineno, char * fmt, ...);
int        no_errors(void);

/*
#define YYERROR_VERBOSE 1
*/

#endif
