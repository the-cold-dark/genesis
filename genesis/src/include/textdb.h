/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/textdb.h
*/

#ifndef _textdb_h_
#define _textdb_h_

#define FORCE_NATIVES  1
#define IGNORE_NATIVES 2

int use_natives;

void compile_cdc_file(FILE * fp);
int text_dump(void);

#endif

