/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/token.h
// ---
// Declarations for the lexer.
*/

#ifndef _token_h_
#define _token_h_

#include <stdio.h>
#include "data.h"

void init_token(void);
void lex_start(list_t * code_lines);
int  yylex(void);
int  is_valid_ident(char * s);
int  cur_lineno(void);

#endif

