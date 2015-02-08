/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <string.h>
#include "util.h"

/* We use MALLOC_DELTA to keep the table sizes at least 32 bytes below a power
 * of two, assuming an Int is four bytes. */
/* HACKNOTE: BAD */
#define MALLOC_DELTA 8
#define INIT_TAB_SIZE (64 - MALLOC_DELTA)

StringTab *string_tab_new_with_size(Long size)
{
    StringTab *new_tab;
    Long i;

    new_tab           = (StringTab*)EMALLOC(StringTab, 1);
    new_tab->tab_size = size;
    new_tab->tab_num  = 0;
    new_tab->tab      = EMALLOC(StringTabEntry, new_tab->tab_size);
    new_tab->hashtab  = EMALLOC(Long, new_tab->tab_size);
    new_tab->blanks   = 0;

    memset(new_tab->tab,      0, sizeof(StringTabEntry)*new_tab->tab_size);
    memset(new_tab->hashtab, -1, sizeof(Long) * new_tab->tab_size);

    for (i = 0; i < new_tab->tab_size; i++)
        new_tab->tab[i].next = i + 1;

    new_tab->tab[new_tab->tab_size - 1].next = -1;

    return new_tab;
}

StringTab *string_tab_new(void)
{
    return string_tab_new_with_size(INIT_TAB_SIZE);
}

void string_tab_free(StringTab *tab)
{
    Int i;

    for (i = 0; i < tab->tab_size; i++) {
        if (tab->tab[i].str)
            string_discard(tab->tab[i].str);
    }
    efree(tab->tab);
    efree(tab->hashtab);
    efree(tab);
}

void string_tab_fixup_hashtab(StringTab *tab, Long num)
{
    Long i, ind;

    for (i = 0; i < num; i++) {
        if (tab->tab[i].str) {
            ind = tab->tab[i].hash % tab->tab_size;
            tab->tab[i].next = tab->hashtab[ind];
            tab->hashtab[ind] = i;
        }
    }
}

static Ident string_tab_from_hash(StringTab *tab, uLong hval, cStr * str) {
    Long ind, new_size;

    /* Look for an existing identifier. */
    ind = tab->hashtab[hval % tab->tab_size];
    while (ind != -1) {
        if (tab->tab[ind].hash == hval &&
            tab->tab[ind].str->len == str->len &&
            strcmp(string_chars(tab->tab[ind].str), string_chars(str)) == 0) {
            tab->tab[ind].refs++;
            return ind;
        }
        ind = tab->tab[ind].next;
    }

    /* Check if we have to resize the table. */
    if (tab->blanks == -1) {

        /* Allocate new space for table. */
        if (tab->tab_size > 4096)
            new_size = tab->tab_size + 4096;
        else
            new_size = tab->tab_size * 2 + MALLOC_DELTA;

        tab->tab = EREALLOC(tab->tab, StringTabEntry, new_size);
        tab->hashtab = EREALLOC(tab->hashtab, Long, new_size);

        /* Make new string of blanks. */
        memset(&(tab->tab[tab->tab_size]), 0, sizeof(StringTabEntry)*(new_size-tab->tab_size));
        for (ind = tab->tab_size; ind < new_size - 1; ind++) {
            tab->tab[ind].next = ind + 1;
        }
        tab->tab[ind].next = -1;
        tab->blanks = tab->tab_size;

        /* Reset hash table. */
        memset(tab->hashtab, -1, sizeof(Long) * new_size);

        ind = tab->tab_size;
        tab->tab_size = new_size;
        string_tab_fixup_hashtab(tab, ind);
    }

    /* Install symbol at first blank. */
    ind = tab->blanks;
    tab->blanks = tab->tab[ind].next;
    tab->tab_num++;
    tab->tab[ind].str = string_dup(str);
    tab->tab[ind].hash = hval;
    tab->tab[ind].refs = 1;
    tab->tab[ind].next = tab->hashtab[hval % tab->tab_size];
    tab->hashtab[hval % tab->tab_size] = ind;

    return ind;
}

Ident string_tab_get(StringTab *tab, char *s) {
    return string_tab_get_length(tab, s, strlen(s));
}

Ident string_tab_get_length(StringTab *tab, char *s, Int len)
{
    uLong hval;
    Int ind;
    cStr *str;

    str = string_from_chars(s, len);
    hval = hash_string(str);
    ind = string_tab_from_hash(tab, hval, str);
    string_discard(str);
    return ind;
}

Ident string_tab_get_string(StringTab *tab, cStr * str) {
    uLong hval;

    hval = hash_string(str);

    return string_tab_from_hash(tab, hval, str);
}

void string_tab_discard(StringTab *tab, Ident id) {
    Long ind, *p;

    tab->tab[id].refs--;

    if (!tab->tab[id].refs) {
        /* Get the hash table thread for this entry. */
        ind = hash_string(tab->tab[id].str) % tab->tab_size;

        /* Free the string. */
        string_discard(tab->tab[id].str);
        tab->tab[id].str = NULL;
        tab->tab[id].hash = 0;

        /* Find the pointer to this entry. */
        for (p = &tab->hashtab[ind]; p && *p != id; p = &tab->tab[*p].next);

        /* Remove this entry and add it to blanks. */
        *p = tab->tab[id].next;
        tab->tab[id].next = tab->blanks;
        tab->blanks = id;
        tab->tab_num--;
    }
}

Ident string_tab_dup(StringTab *tab, Ident id) {
    tab->tab[id].refs++;

    if (!tab->tab[id].str)
      panic("ident_dup tried to duplicate freed name.");

    return id;
}

char *string_tab_name(StringTab *tab, Ident id) {
    return string_chars(tab->tab[id].str);
}

cStr *string_tab_name_str(StringTab *tab, Ident id)
{
    return tab->tab[id].str;
}

char *string_tab_name_size(StringTab *tab, Ident id, Int *sz)
{
    cStr *str = tab->tab[id].str;

    *sz = str->len;
    return string_chars(str);
}

uLong string_tab_hash(StringTab *tab, Ident id) {
    return tab->tab[id].hash;
}
