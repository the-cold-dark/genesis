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

#endif

