/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include "quickhash.h"

#define MALLOC_DELTA			 0
#define HASHTAB_STARTING_SIZE		 128

INTERNAL void double_hashtab_size(Hash * hash);
INTERNAL void insert_key(Hash * hash, Int i);

Hash * hash_new_with(cList *keys) {
    Hash *cnew;
    Int i, j;

    /* Construct a new hash */
    cnew = EMALLOC(Hash, 1);
    cnew->keys = list_dup(keys);

    /* Calculate initial size of chain and hash table. */
    cnew->hashtab_size = HASHTAB_STARTING_SIZE;
    while (cnew->hashtab_size < keys->len)
	cnew->hashtab_size = cnew->hashtab_size * 2 + MALLOC_DELTA;

    /* Initialize chain entries and hash table. */
    cnew->links = EMALLOC(Int, cnew->hashtab_size);
    cnew->hashtab = EMALLOC(Int, cnew->hashtab_size);
    for (i = 0; i < cnew->hashtab_size; i++) {
	cnew->links[i] = -1;
	cnew->hashtab[i] = -1;
    }

    /* Insert the keys into the hash table, eliminating duplicates. */
    i = j = 0;
    while (i < cnew->keys->len) {
	if (i != j)
	    cnew->keys->el[j] = cnew->keys->el[i];
	if (hash_find(cnew, &keys->el[i]) == F_FAILURE)
	    insert_key(cnew, j++);
	else
	    data_discard(&cnew->keys->el[i]);
	i++;
    }
    cnew->keys->len = j;

    cnew->refs = 1;
    return cnew;
}

Hash * hash_new(int size)
{
    cList * keys;
    Hash  * out;

    keys = list_new(size);
    out = hash_new_with(keys);
    list_discard(keys);
    return out;
}

Hash * hash_dup(Hash * hash)
{
    hash->refs++;
    return hash;
}

void hash_discard(Hash * hash)
{
    hash->refs--;
    if (!hash->refs) {
	list_discard(hash->keys);
	efree(hash->links);
	efree(hash->hashtab);
	efree(hash);
    }
}

Hash * hash_add(Hash * hash, cData * key) {
    Int pos;

#if 0
    hash = hash_prep(hash);
#endif

    pos = hash_find(hash, key);
    if (pos != F_FAILURE)
        return hash;

    /* Add the key */
    hash->keys = list_add(hash->keys, key);

    /* Check if we should resize the hash table. */
    if (hash->keys->len > hash->hashtab_size)
	double_hashtab_size(hash);
    else
	insert_key(hash, hash->keys->len - 1);
    return hash;
}

INTERNAL void insert_key(Hash * hash, Int i) {
    Int ind;

    ind = data_hash(&hash->keys->el[i]) % hash->hashtab_size;
    hash->links[i] = hash->hashtab[ind];
    hash->hashtab[ind] = i;
}

Int hash_find(Hash * hash, cData *key) {
    Int ind, i;

    ind = data_hash(key) % hash->hashtab_size;
    for (i = hash->hashtab[ind]; i != -1; i = hash->links[i]) {
	if (data_cmp(&hash->keys->el[i], key) == 0)
	    return i;
    }

    return F_FAILURE;
}

INTERNAL void double_hashtab_size(Hash * hash)
{
    Int i;

    hash->hashtab_size = hash->hashtab_size * 2 + MALLOC_DELTA;
    hash->links = EREALLOC(hash->links, Int, hash->hashtab_size);
    hash->hashtab = EREALLOC(hash->hashtab, Int, hash->hashtab_size);
    for (i = 0; i < hash->hashtab_size; i++) {
	hash->links[i] = -1;
	hash->hashtab[i] = -1;
    }
    for (i = 0; i < hash->keys->len; i++)
	insert_key(hash, i);
}


