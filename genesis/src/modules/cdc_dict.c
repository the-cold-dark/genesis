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

#include "config.h"
#include "defs.h"
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "memory.h"

void native_dict_keys(void) {
    data_t * args;
    list_t * keys;

    if (!func_init_1(&args, DICT))
	return;

    keys = dict_keys(args[0].u.dict);
    pop(1);
    push_list(keys);
    list_discard(keys);
}

void native_dict_add(void) {
    data_t * args;

    if (!func_init_3(&args, DICT, 0, 0))
	return;

    anticipate_assignment();
    args[0].u.dict = dict_add(args[0].u.dict, &args[1], &args[2]);
    pop(2);
}

void native_dict_del(void) {
    data_t * args;

    if (!func_init_2(&args, DICT, 0))
	return;

    if (!dict_contains(args[0].u.dict, &args[1])) {
	cthrow(keynf_id, "Key (%D) is not in the dictionary.", &args[1]);
    } else {
	anticipate_assignment();
	args[0].u.dict = dict_del(args[0].u.dict, &args[1]);
	pop(1);
    }
}

void native_dict_contains(void) {
    data_t * args;
    int      val;

    if (!func_init_2(&args, DICT, 0))
	return;

    val = dict_contains(args[0].u.dict, &args[1]);
    pop(2);
    push_int(val);
}

