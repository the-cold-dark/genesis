/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/dump.h
// ---
// Declarations for binary and text database dumps.
*/

#ifndef _dump_h_
#define _dump_h_

#include <stdio.h>

int binary_dump(void);
int text_dump(void);
void text_dump_read(FILE *fp);

#endif

