/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/lookup.h
// ---
// Location database routines.
*/

#ifndef LOOKUP_H
#define LOOKUP_H

#ifndef _did_sys_types_
#define _did_sys_types_
#include <sys/types.h>
#endif

void lookup_open(char *name, int cnew);
void lookup_close(void);
void lookup_sync(void);
int lookup_retrieve_objnum(long objnum, off_t *offset, int *size);
int lookup_store_objnum(long objnum, off_t offset, int size);
int lookup_remove_objnum(long objnum);
long lookup_first_objnum(void);
long lookup_next_objnum(void);
int lookup_retrieve_name(long name, long *objnum);
int lookup_store_name(long name, long objnum);
int lookup_remove_name(long name);
long lookup_first_name(void);
long lookup_next_name(void);

#endif

