/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/data.h
// ---
// Declarations for ColdC data.
*/

#ifndef _data_h_
#define _data_h_

#include "cdc_types.h"

/* Buffer contents must be between 0 and 255 inclusive, even if an unsigned
 * char can hold other values. */
#define OCTET_VALUE(n) (((unsigned long) (n)) & ((1 << 8) - 1))

int           data_cmp(data_t * d1, data_t * d2);
int           data_true(data_t * data);
unsigned long data_hash(data_t * d);
void          data_dup(data_t * dest, data_t * src);
void          data_discard(data_t * data);
string_t    * data_tostr(data_t * data);
string_t    * data_to_literal(data_t * data);
string_t    * data_add_list_literal_to_str(string_t * str, list_t * list);
string_t    * data_add_literal_to_str(string_t * str, data_t * data);
long          data_type_id(int type);

#endif

