/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/match.h
// ---
// Declarations for the pattern matcher.
*/

#ifndef _match_h_
#define _match_h_

#include "data.h"

void     init_match(void);
list_t * match_template(char * ctemplate, char * s);
list_t * match_pattern(char * pattern, char * s);
list_t * match_regexp(string_t * reg, char * s, int sensitive);
list_t * regexp_matches(string_t * reg, char * s, int sensitive);
int parse_strsed_args(char * args, int * global, int * sensitive);
string_t * strsed(string_t * reg,  /* the regexp string */
                  string_t * ss,   /* the string to match against */
                  string_t * rs,   /* the replacement string */
                  int global,      /* globally match? */
                  int sensitive,   /* case sensitive? */
                  int mult,        /* size multiplier */           
                  int * err);      /* did we have a boo boo? */
string_t * strfmt(string_t * str, data_t * args, int argc);

#endif

