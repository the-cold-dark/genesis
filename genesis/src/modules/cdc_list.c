/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_list.c
// ---
// List Manipulation module.
*/

#define NATIVE_MODULE "$list"

#include "config.h"
#include "defs.h"
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "memory.h"

NATIVE_METHOD(listlen) {
    INIT_1_ARG(LIST)

    RETURN_INTEGER(list_length(_LIST(ARG)));
}

NATIVE_METHOD(sublist) {
    int start, span, list_len;

    INIT_2_OR_3_ARGS(LIST, INTEGER, INTEGER)

    list_len = list_length(_LIST(ARG1));
    start = _INT(ARG2) - 1;
    span = (argc == 3) ? _INT(ARG3) : list_len - start;

    /* Make sure range is in bounds. */
    if (start < 0)
        THROW((range_id, "Start (%d) less than one", start + 1))
    else if (span < 0)
        THROW((range_id, "Sublist length (%d) less than zero", span))
    else if (start + span > list_len)
        THROW((range_id, "Sublist extends to %d, past end of list (length %d)",
              start + span, list_len))

    RETURN_LIST(list_sublist(_LIST(ARG1), start, span))
}

NATIVE_METHOD(insert) {
    int pos, list_len;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(LIST)
    INIT_ARG2(INTEGER)

    pos = _INT(ARG2) - 1;
    list_len = list_length(_LIST(ARG1));

    if (pos < 0)
        THROW((range_id, "Position (%d) less than one", pos + 1))
    else if (pos > list_len)
        THROW((range_id, "Position (%d) beyond end of list (length %d)",
              pos + 1, list_len))

    RETURN_LIST(list_insert(_LIST(ARG1), pos, &args[2]))
}

NATIVE_METHOD(replace) {
    int pos, list_len;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(LIST)
    INIT_ARG2(INTEGER)

    list_len = list_length(args[0].u.list);
    pos = args[1].u.val - 1;

    if (pos < 0)
        THROW((range_id, "Position (%d) less than one", pos + 1))
    else if (pos > list_len - 1)
        THROW((range_id, "Position (%d) greater than length of list (%d)",
              pos + 1, list_len))

    RETURN_LIST(list_replace(args[0].u.list, pos, &args[2]))
}

NATIVE_METHOD(delete) {
    int pos, list_len;

    INIT_2_ARGS(LIST, INTEGER)

    list_len = list_length(args[0].u.list);
    pos = args[1].u.val - 1;

    if (pos < 0)
        THROW((range_id, "Position (%d) less than one", pos + 1))
    else if (pos > list_len - 1)
        THROW((range_id, "Position (%d) greater than length of list (%d)",
              pos + 1, list_len))

    RETURN_LIST(list_delete(args[0].u.list, pos))
}

NATIVE_METHOD(setadd) {
    DEF_args;

    INIT_ARGC(ARG_COUNT, 2, "two")
    INIT_ARG1(LIST)

    RETURN_LIST(list_setadd(args[0].u.list, &args[1]))
}

NATIVE_METHOD(setremove) {
    DEF_args;
    
    INIT_ARGC(ARG_COUNT, 2, "two") 
    INIT_ARG1(LIST)

    RETURN_LIST(list_setremove(args[0].u.list, &args[1]))
}

NATIVE_METHOD(union) {
    INIT_2_ARGS(LIST, LIST)

    RETURN_LIST(list_union(args[0].u.list, args[1].u.list))
}

