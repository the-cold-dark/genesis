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

Dict * dict_new(list_t * keys, list_t * values);
Dict * dict_new_empty(void);
Dict * dict_from_slices(list_t * slices);
Dict * dict_dup(Dict * dict);
void dict_discard(Dict * dict);
int dict_cmp(Dict * dict1, Dict * dict2);
Dict * dict_add(Dict * dict, data_t * key, data_t * value);
Dict * dict_del(Dict * dict, data_t * key);
long dict_find(Dict * dict, data_t * key, data_t * ret);
int dict_contains(Dict * dict, data_t * key);
list_t * dict_keys(Dict * dict);
list_t * dict_key_value_pair(Dict * mapping, int i);
int dict_size(Dict * dict);
string_t * dict_add_literal_to_str(string_t * str, Dict * dict);

#endif

