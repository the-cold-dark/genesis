/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_data_h
#define cdc_data_h

#include "cdc_types.h"

/* Buffer contents must be between 0 and 255 inclusive, even if an unsigned
 * char can hold other values. */
#define OCTET_VALUE(n) (((uLong) (n)) & ((1 << 8) - 1))

Int     data_cmp(cData * d1, cData * d2);
Int     data_true(cData * data);
uLong   data_hash(cData * d);
void    data_dup(cData * dest, cData * src);
void    data_discard(cData * data);
cStr  * data_tostr(cData * data);
cStr  * data_to_literal(cData * data, Bool objnames);
cStr  * data_add_list_literal_to_str(cStr * str, cList * list, Bool objnames);
cStr  * data_add_literal_to_str(cStr * str, cData * data, Bool objnames);
Long    data_type_id(Int type);

char  * data_from_literal(cData *d, char *s);

#endif

