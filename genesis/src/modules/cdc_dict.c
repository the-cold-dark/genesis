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
    INIT_1_ARG(DICT);

    RETURN_LIST(dict_keys(args[0].u.dict));
}

NATIVE_METHOD(dict_add) {
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(DICT);

    RETURN_DICT(dict_add(args[0].u.dict, &args[1], &args[2]));
}

NATIVE_METHOD(dict_del) {
    DEF_args;

    INIT_ARGC(ARG_COUNT, 2, "two");
    INIT_ARG1(DICT);

    if (!dict_contains(args[0].u.dict, &args[1]))
        THROW((keynf_id, "Key (%D) is not in the dictionary.", &args[1]));

    RETURN_DICT(dict_del(args[0].u.dict, &args[1]));
}

NATIVE_METHOD(dict_contains) {
    DEF_args;

    INIT_ARGC(ARG_COUNT, 2, "two");
    INIT_ARG1(DICT);

    RETURN_INTEGER(dict_contains(args[0].u.dict, &args[1]));
}

