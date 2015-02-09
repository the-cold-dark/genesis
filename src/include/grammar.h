/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_grammar_h
#define cdc_grammar_h

#include <stdarg.h>

Method * compile(Obj *object, cList * code, cList ** error_ret);
void       compiler_error(Int lineno, char * fmt, ...);
Int        no_errors(void);

#if DISABLED
#define YYERROR_VERBOSE 1
#endif

#endif
