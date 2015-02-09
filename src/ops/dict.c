/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include "functions.h"
#include "execute.h"

COLDC_FUNC(dict_values) {
    cData * args;
    cList * values;

    if (!func_init_1(&args, DICT))
        return;

    values = dict_values(DICT1);
    pop(1);
    push_list(values);
    list_discard(values);
}

COLDC_FUNC(dict_keys) {
    cData * args;
    cList * keys;

    if (!func_init_1(&args, DICT))
        return;

    keys = dict_keys(DICT1);
    pop(1);
    push_list(keys);
    list_discard(keys);
}

COLDC_FUNC(dict_add) {
    cData * args;

    if (!func_init_3(&args, DICT, 0, 0))
        return;

    anticipate_assignment();
    args[0].u.dict = dict_add(args[0].u.dict, &args[1], &args[2]);
    pop(2);
}

COLDC_FUNC(dict_del) {
    cData * args;

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

COLDC_FUNC(dict_contains) {
    cData * args;
    Int      val;

    if (!func_init_2(&args, DICT, 0))
        return;

    val = dict_contains(args[0].u.dict, &args[1]);
    pop(2);
    push_int(val);
}

COLDC_FUNC(dict_union) {
    cData * args;
    cDict * dict1, * dict2, *d;

    if (!func_init_2(&args, DICT, DICT))
        return;

    dict1 = dict_dup(DICT1);
    dict2 = dict_dup(DICT2);
    pop(2);

    /* dict_union will discard the dicts */
    d = dict_union(dict1, dict2);

    push_dict(d);
    dict_discard(d);
}

