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

void lookup_open(char *name, int cnew);
void lookup_close(void);
void lookup_sync(void);
int lookup_retrieve_dbref(long dbref, off_t *offset, int *size);
int lookup_store_dbref(long dbref, off_t offset, int size);
int lookup_remove_dbref(long dbref);
long lookup_first_dbref(void);
long lookup_next_dbref(void);
int lookup_retrieve_name(long name, long *dbref);
int lookup_store_name(long name, long dbref);
int lookup_remove_name(long name);
long lookup_first_name(void);
long lookup_next_name(void);

#endif

