/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_dict_h
#define cdc_dict_h

#include "cdc_types.h"

cDict * dict_new(cList * keys, cList * values);
cDict * dict_new_empty(void);
cDict * dict_from_slices(cList * slices);
cDict * dict_dup(cDict * dict);
void dict_discard(cDict * dict);
Int dict_cmp(cDict * dict1, cDict * dict2);
cDict * dict_add(cDict * dict, cData * key, cData * value);
cDict * dict_del(cDict * dict, cData * key);
cDict * dict_prep(cDict *);
Long dict_find(cDict * dict, cData * key, cData * ret);
Int dict_contains(cDict * dict, cData * key);
cList * dict_keys(cDict * dict);
cList * dict_values(cDict * dict);
cList * dict_key_value_pair(cDict * mapping, Int i);
Int dict_size(cDict * dict);
cStr * dict_add_literal_to_str(cStr * str, cDict * dict, int flags);
cDict *dict_union (cDict *d1, cDict *d2);


#endif

