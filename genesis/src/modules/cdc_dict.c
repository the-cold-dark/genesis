/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_dict.c
// ---
// Dictionary manipulation module.
*/

#define NATIVE_MODULE "$dictionary"

#include "config.h"
#include "defs.h"
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "memory.h"

NATIVE_METHOD(dict_keys) {
    list_t * list;

    INIT_1_ARG(DICT);

    list = dict_keys(DICT1);

    CLEAN_RETURN_LIST(list);
}

/* ugh; what we do to keep from copying */
NATIVE_METHOD(dict_add) {
    dict_t * dict;
    data_t   arg1, arg2;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(DICT);

    dict = dict_dup(DICT1);
    data_dup(&arg1, &args[1]);
    data_dup(&arg2, &args[2]);

    CLEAN_STACK();

    anticipate_assignment();
    dict = dict_add(dict, &arg1, &arg2);
    data_discard(&arg1);
    data_discard(&arg2);

    RETURN_DICT(dict);
}

NATIVE_METHOD(dict_del) {
    data_t arg1;
    dict_t * dict;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 2, "two");
    INIT_ARG1(DICT);

    if (!dict_contains(DICT1, &args[1]))
        THROW((keynf_id, "Key (%D) is not in the dictionary.", &args[1]));

    dict = dict_dup(DICT1);
    data_dup(&arg1, &args[1]);

    CLEAN_STACK();
    anticipate_assignment();

    dict = dict_del(dict, &arg1);
    data_discard(&arg1);

    RETURN_DICT(dict);
}

NATIVE_METHOD(dict_contains) {
    int val;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 2, "two");
    INIT_ARG1(DICT);

    val = dict_contains(DICT1, &args[1]);

    CLEAN_RETURN_INTEGER(val);
}

