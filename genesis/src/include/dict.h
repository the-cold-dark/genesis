/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/dict.h
// ---
// Declarations for ColdC dictionaries.
*/

#ifndef _dict_h_
#define _dict_h_

#include "cdc_types.h"

#if 0
#include "data.h"
#endif

dict_t * dict_new(list_t * keys, list_t * values);
dict_t * dict_new_empty(void);
dict_t * dict_from_slices(list_t * slices);
dict_t * dict_dup(dict_t * dict);
void dict_discard(dict_t * dict);
int dict_cmp(dict_t * dict1, dict_t * dict2);
dict_t * dict_add(dict_t * dict, data_t * key, data_t * value);
dict_t * dict_del(dict_t * dict, data_t * key);
dict_t * dict_prep(dict_t *);
long dict_find(dict_t * dict, data_t * key, data_t * ret);
int dict_contains(dict_t * dict, data_t * key);
list_t * dict_keys(dict_t * dict);
list_t * dict_key_value_pair(dict_t * mapping, int i);
int dict_size(dict_t * dict);
string_t * dict_add_literal_to_str(string_t * str, dict_t * dict);

#endif

