/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include "dict.h"

#define MALLOC_DELTA			 0
#define HASHTAB_STARTING_SIZE		 8

INTERNAL void insert_key(cDict *dict, Int i);
INTERNAL Int search(cDict *dict, cData *key);
INTERNAL void double_hashtab_size(cDict *dict);

cDict *dict_new(cList *keys, cList *values)
{
    cDict *cnew;
    Int i, j;

    /* Construct a new dictionary. */
    cnew = EMALLOC(cDict, 1);
    cnew->keys = list_dup(keys);
    cnew->values = list_dup(values);

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
	if (i != j) {
	    cnew->keys->el[j] = cnew->keys->el[i];
	    cnew->values->el[j] = cnew->values->el[i];
	}
	if (search(cnew, &keys->el[i]) == F_FAILURE) {
	    insert_key(cnew, j++);
	} else {
	    data_discard(&cnew->keys->el[i]);
	    data_discard(&cnew->values->el[i]);
	}
	i++;
    }
    cnew->keys->len = cnew->values->len = j;

    cnew->refs = 1;
    return cnew;
}

cDict *dict_new_empty(void)
{
    cList *l1, *l2;
    cDict *dict;

    l1 = list_new(0);
    l2 = list_new(0);
    dict = dict_new(l1, l2);
    list_discard(l1);
    list_discard(l2);
    return dict;
}

cDict *dict_from_slices(cList *slices)
{
    cList *keys, *values;
    cDict *dict;
    cData *d;

    /* Make lists for keys and values. */
    keys = list_new(list_length(slices));
    values = list_new(list_length(slices));

    for (d = list_first(slices); d; d = list_next(slices, d)) {
	if (d->type != LIST || list_length(d->u.list) != 2) {
	    /* Invalid slice.  Throw away what we had and return NULL. */
	    list_discard(keys);
	    list_discard(values);
	    return NULL;
	}
	keys = list_add(keys, list_elem(d->u.list, 0));
	values = list_add(values, list_elem(d->u.list, 1));
    }

    /* Slices were all valid; return new dict. */
    dict = dict_new(keys, values);
    list_discard(keys);
    list_discard(values);
    return dict;
}

cDict *dict_dup(cDict *dict)
{
    dict->refs++;
    return dict;
}

void dict_discard(cDict *dict)
{
    dict->refs--;
    if (!dict->refs) {
	list_discard(dict->keys);
	list_discard(dict->values);
	efree(dict->links);
	efree(dict->hashtab);
	efree(dict);
    }
}

Int dict_cmp(cDict *dict1, cDict *dict2)
{
    if (list_cmp(dict1->keys, dict2->keys) == 0 &&
	list_cmp(dict1->values, dict2->values) == 0)
	return 0;
    else
	return 1;
}

cDict *dict_add(cDict *dict, cData *key, cData *value)
{
    Int pos;

    dict = dict_prep(dict);

    /* Just replace the value for the key if it already exists. */
    pos = search(dict, key);
    if (pos != F_FAILURE) {
	dict->values = list_replace(dict->values, pos, value);
	return dict;
    }

    /* Add the key and value to the list. */
    dict->keys = list_add(dict->keys, key);
    dict->values = list_add(dict->values, value);

    /* Check if we should resize the hash table. */
    if (dict->keys->len > dict->hashtab_size)
	double_hashtab_size(dict);
    else
	insert_key(dict, dict->keys->len - 1);
    return dict;
}

/* Error-checking is the caller's responsibility; this routine assumes that it
 * will find the key in the dictionary. */
cDict *dict_del(cDict *dict, cData *key)
{
    Int ind, *ip, i = -1, j;

    dict = dict_prep(dict);

    /* Search for a pointer to the key, either in the hash table entry or in
     * the chain links. */
    ind = data_hash(key) % dict->hashtab_size;
    for (ip = &dict->hashtab[ind];; ip = &dict->links[*ip]) {
	i = *ip;
	if (data_cmp(&dict->keys->el[i], key) == 0)
	    break;
    }

    /* Delete the element from the keys and values lists. */
    dict->keys = list_delete(dict->keys, i);
    dict->values = list_delete(dict->values, i);

    /* Replace the pointer to the key index with the next link. */
    *ip = dict->links[i];

    /* Copy the links beyond i backward. */
    MEMMOVE(dict->links + i, dict->links + i + 1, dict->keys->len - i);
    dict->links[dict->keys->len] = -1;

    /* Since we've renumbered all the elements beyond i, we have to check
     * all the links and hash table entries.  If they're greater than i,
     * decrement them.  Skip this step if the element we removed was the last
     * one. */
    if (i < dict->keys->len) {
	for (j = 0; j < dict->keys->len; j++) {
	    if (dict->links[j] > i)
		dict->links[j]--;
	}
	for (j = 0; j < dict->hashtab_size; j++) {
	    if (dict->hashtab[j] > i)
		dict->hashtab[j]--;
	}
    }

    return dict;
}

Long dict_find(cDict *dict, cData *key, cData *ret)
{
    Int pos;

    pos = search(dict, key);
    if (pos == F_FAILURE)
	return keynf_id;

    data_dup(ret, &dict->values->el[pos]);
    return NOT_AN_IDENT;
}

Int dict_contains(cDict *dict, cData *key)
{
    Int pos;

    pos = search(dict, key);
    return (pos != F_FAILURE);
}

cList *dict_values(cDict *dict)
{
    return list_dup(dict->values);
}

cList *dict_keys(cDict *dict)
{
    return list_dup(dict->keys);
}

cList *dict_key_value_pair(cDict *dict, Int i)
{
    cList *l;

    if (i >= dict->keys->len)
	return NULL;
    l = list_new(2);
    l->len = 2;
    data_dup(&l->el[0], &dict->keys->el[i]);
    data_dup(&l->el[1], &dict->values->el[i]);
    return l;
}

cStr *dict_add_literal_to_str(cStr *str, cDict *dict, int flags)
{
    Int i;

    str = string_add_chars(str, "#[", 2);
    for (i = 0; i < dict->keys->len; i++) {
	str = string_addc(str, '[');
	str = data_add_literal_to_str(str, &dict->keys->el[i], flags);
	str = string_add_chars(str, ", ", 2);
	str = data_add_literal_to_str(str, &dict->values->el[i], flags);
	str = string_addc(str, ']');
	if (i < dict->keys->len - 1)
	    str = string_add_chars(str, ", ", 2);
    }
    return string_addc(str, ']');
}

cDict *dict_prep(cDict *dict) {
    cDict *cnew;

    if (dict->refs == 1)
	return dict;

    /* Duplicate the old dictionary. */
    cnew = EMALLOC(cDict, 1);
    cnew->keys = list_dup(dict->keys);
    cnew->values = list_dup(dict->values);
    cnew->hashtab_size = dict->hashtab_size;
    cnew->links = EMALLOC(Int, cnew->hashtab_size);
    MEMCPY(cnew->links, dict->links, cnew->hashtab_size);
    cnew->hashtab = EMALLOC(Int, cnew->hashtab_size);
    MEMCPY(cnew->hashtab, dict->hashtab, cnew->hashtab_size);
    dict->refs--;
    cnew->refs = 1;
    return cnew;
}

INTERNAL void insert_key(cDict *dict, Int i)
{
    Int ind;

    ind = data_hash(&dict->keys->el[i]) % dict->hashtab_size;
    dict->links[i] = dict->hashtab[ind];
    dict->hashtab[ind] = i;
}

INTERNAL Int search(cDict *dict, cData *key) {
    Int ind, i;

    ind = data_hash(key) % dict->hashtab_size;
    for (i = dict->hashtab[ind]; i != -1; i = dict->links[i]) {
	if (data_cmp(&dict->keys->el[i], key) == 0)
	    return i;
    }

    return F_FAILURE;
}

Int dict_size(cDict *dict)
{
    return list_length(dict->keys);
}

INTERNAL void double_hashtab_size(cDict *dict)
{
    Int i;

    dict->hashtab_size = dict->hashtab_size * 2 + MALLOC_DELTA;
    dict->links = EREALLOC(dict->links, Int, dict->hashtab_size);
    dict->hashtab = EREALLOC(dict->hashtab, Int, dict->hashtab_size);
    for (i = 0; i < dict->hashtab_size; i++) {
	dict->links[i] = -1;
	dict->hashtab[i] = -1;
    }
    for (i = 0; i < dict->keys->len; i++)
	insert_key(dict, i);
}

/* WARNING: This will discard both arguments! */
cDict *dict_union (cDict *d1, cDict *d2) {
    int i, pos;
    Bool swap;

    if (d2->keys->len > d1->keys->len) {
        cDict *t;
        t=d2; d2=d1; d1=t;
        swap = NO;
    } else {
        swap = YES;
    }

    d1=dict_prep(d1);

    for (i=0; i<d2->keys->len; i++) {
        cData *key=&d2->keys->el[i], *value=&d2->values->el[i];

        pos = search(d1, key);

        /* forget the add if it's already there */
        if (pos != F_FAILURE) {
            /* ... but if the args are in the wrong order, we
               want to overwrite the key */
            if (swap)
                d1->values = list_replace(d1->values, pos, value);
            continue;
        }

        /* Add the key and value to the list. */
        d1->keys = list_add(d1->keys, key);
        d1->values = list_add(d1->values, value);

        /* Check if we should resize the hash table. */
        if (d1->keys->len > d1->hashtab_size)
            double_hashtab_size(d1);
        else
            insert_key(d1, d1->keys->len - 1);
    }
    dict_discard(d2);
    return d1;
}

