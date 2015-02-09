/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef quickhash_h
#define quickhash_h

#include "cdc_types.h"

typedef cDict Hash;

Hash * hash_new_with(cList *keys);
Hash * hash_new(int size);
Hash * hash_dup(Hash * hash);
void hash_discard(Hash * hash);
Hash * hash_add(Hash * hash, cData * key);
Int hash_find(Hash * hash, cData * key);

#endif

