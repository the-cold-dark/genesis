/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/dbpack.h
// ---
// Declarations for packing objects in the database.
*/

#ifndef _dbpack_h_
#define _dbpack_h_

#include <stdio.h>
#include "object.h"

void pack_object(object_t * obj, FILE * fp);
void unpack_object(object_t * obj, FILE * fp);
int  size_object(object_t * obj);

void write_long(long n, FILE * fp);
long read_long(FILE * fp);
int  size_long(long n);

#endif

