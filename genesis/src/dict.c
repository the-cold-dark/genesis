/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: dict.c
// ---
// Routines for manipulating dictionaries.
*/

#include "config.h"
#include "defs.h"

#include "y.tab.h"
#include "dict.h"
#include "memory.h"
#include "ident.h"

#define MALLOC_DELTA			 5
#define HASHTAB_STARTING_SIZE		(32 - MALLOC_DELTA)

static Dict *prepare_to_modify(Dict *dict);
static void insert_key(Dict *dict, int i);
static int search(Dict *dict, data_t *key);
static void double_hashtab_size(Dict *dict);

Dict *dict_new(list_t *keys, list_t *values)
{
    Dict *cnew;
    int i, j;

    /* Construct a new dictionary. */
    cnew = EMALLOC(Dict, 1);
    cnew->keys = list_dup(keys);
    cnew->values = list_dup(values);

    /* Calculate initial size of chain and hash table. */
    cnew->hashtab_size = HASHTAB_STARTING_SIZE;
    while (cnew->hashtab_size < keys->len)
	cnew->hashtab_size = cnew->hashtab_size * 2 + MALLOC_DELTA;

    /* Initialize chain entries and hash table. */
    cnew->links = EMALLOC(int, cnew->hashtab_size);
    cnew->hashtab = EMALLOC(int, cnew->hashtab_size);
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
	if (search(cnew, &keys->el[i]) == -1) {
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

Dict *dict_new_empty(void)
{
    list_t *l1, *l2;
    Dict *dict;

    l1 = list_new(0);
    l2 = list_new(0);
    dict = dict_new(l1, l2);
    list_discard(l1);
    list_discard(l2);
    return dict;
}

Dict *dict_from_slices(list_t *slices)
{
    list_t *keys, *values;
    Dict *dict;
    data_t *d;

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

Dict *dict_dup(Dict *dict)
{
    dict->refs++;
    return dict;
}

void dict_discard(Dict *dict)
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

int dict_cmp(Dict *dict1, Dict *dict2)
{
    if (list_cmp(dict1->keys, dict2->keys) == 0 &&
	list_cmp(dict1->values, dict2->values) == 0)
	return 0;
    else
	return 1;
}

Dict *dict_add(Dict *dict, data_t *key, data_t *value)
{
    int pos;

    dict = prepare_to_modify(dict);

    /* Just replace the value for the key if it already exists. */
    pos = search(dict, key);
    if (pos != -1) {
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
Dict *dict_del(Dict *dict, data_t *key)
{
    int ind, *ip, i = -1, j;

    dict = prepare_to_modify(dict);

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

long dict_find(Dict *dict, data_t *key, data_t *ret)
{
    int pos;

    pos = search(dict, key);
    if (pos == -1)
	return keynf_id;

    data_dup(ret, &dict->values->el[pos]);
    return NOT_AN_IDENT;
}

int dict_contains(Dict *dict, data_t *key)
{
    int pos;

    pos = search(dict, key);
    return (pos != -1);
}

list_t *dict_keys(Dict *dict)
{
    return list_dup(dict->keys);
}

list_t *dict_key_value_pair(Dict *dict, int i)
{
    list_t *l;

    if (i >= dict->keys->len)
	return NULL;
    l = list_new(2);
    l->len = 2;
    data_dup(&l->el[0], &dict->keys->el[i]);
    data_dup(&l->el[1], &dict->values->el[i]);
    return l;
}

string_t *dict_add_literal_to_str(string_t *str, Dict *dict)
{
    int i;

    str = string_add_chars(str, "#[", 2);
    for (i = 0; i < dict->keys->len; i++) {
	str = string_addc(str, '[');
	str = data_add_literal_to_str(str, &dict->keys->el[i]);
	str = string_add_chars(str, ", ", 2);
	str = data_add_literal_to_str(str, &dict->values->el[i]);
	str = string_addc(str, ']');
	if (i < dict->keys->len - 1)
	    str = string_add_chars(str, ", ", 2);
    }	
    return string_addc(str, ']');
}

static Dict *prepare_to_modify(Dict *dict)
{
    Dict *cnew;

    if (dict->refs == 1)
	return dict;

    /* Duplicate the old dictionary. */
    cnew = EMALLOC(Dict, 1);
    cnew->keys = list_dup(dict->keys);
    cnew->values = list_dup(dict->values);
    cnew->hashtab_size = dict->hashtab_size;
    cnew->links = EMALLOC(int, cnew->hashtab_size);
    MEMCPY(cnew->links, dict->links, cnew->hashtab_size);
    cnew->hashtab = EMALLOC(int, cnew->hashtab_size);
    MEMCPY(cnew->hashtab, dict->hashtab, cnew->hashtab_size);
    dict->refs--;
    cnew->refs = 1;
    return cnew;
}

static void insert_key(Dict *dict, int i)
{
    int ind;

    ind = data_hash(&dict->keys->el[i]) % dict->hashtab_size;
    dict->links[i] = dict->hashtab[ind];
    dict->hashtab[ind] = i;
}

static int search(Dict *dict, data_t *key)
{
    int ind, i;

    ind = data_hash(key) % dict->hashtab_size;
    for (i = dict->hashtab[ind]; i != -1; i = dict->links[i]) {
	if (data_cmp(&dict->keys->el[i], key) == 0)
	    return i;
    }

    return -1;
}

int dict_size(Dict *dict)
{
    return list_length(dict->keys);
}

static void double_hashtab_size(Dict *dict)
{
    int i;

    dict->hashtab_size = dict->hashtab_size * 2 + MALLOC_DELTA;
    dict->links = EREALLOC(dict->links, int, dict->hashtab_size);
    dict->hashtab = EREALLOC(dict->hashtab, int, dict->hashtab_size);
    for (i = 0; i < dict->hashtab_size; i++) {
	dict->links[i] = -1;
	dict->hashtab[i] = -1;
    }
    for (i = 0; i < dict->keys->len; i++)
	insert_key(dict, i);
}

