/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/binarydb.h
// ---
// Declarations for dbm chunking routines.
*/

#ifndef _binarydb_h_
#define _binarydb_h_

#include "object.h"

void   init_binary_db(void);
void   init_new_db(void);
int    init_db(int force_textdump);
int    db_get(object_t * object, long name);
int    db_put(object_t * object, long name);
int    db_check(long name);
int    db_del(long name);
char * db_traverse_first(void);
char * db_traverse_next(void);
int    db_backup(char * out);
void   db_close(void);
void   db_flush(void);
void   init_core_objects(void);

#endif

