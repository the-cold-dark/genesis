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
    int len;

    INIT_1_ARG(LIST);

    len = list_length(LIST1);

    CLEAN_RETURN_INTEGER(len);
}

NATIVE_METHOD(sublist) {
    int      start,
             span,
             len;
    list_t * list;

    INIT_2_OR_3_ARGS(LIST, INTEGER, INTEGER)

    len = list_length(LIST1);
    start = INT2 - 1;
    span = (argc == 3) ? INT3 : len - start;

    /* Make sure range is in bounds. */
    if (start < 0)
        THROW((range_id, "Start (%d) less than one", start + 1))
    else if (span < 0)
        THROW((range_id, "Sublist length (%d) less than zero", span))
    else if (start + span > len)
        THROW((range_id, "Sublist extends to %d, past end of list (length %d)",
              start + span, len))

    list = list_dup(LIST1);

    CLEAN_STACK();
    anticipate_assignment();

    list = list_sublist(list, start, span);

    RETURN_LIST(list);
}

NATIVE_METHOD(insert) {
    int      pos,
             len;
    list_t * list;
    data_t   data;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(LIST)
    INIT_ARG2(INTEGER)

    pos = INT2 - 1;
    len = list_length(LIST1);

    if (pos < 0)
        THROW((range_id, "Position (%d) less than one", pos + 1))
    else if (pos > len)
        THROW((range_id, "Position (%d) beyond end of list (length %d)",
              pos + 1, len))

    data_dup(&data, &args[2]);
    list = list_dup(LIST1);

    CLEAN_STACK();
    anticipate_assignment();

    list = list_insert(list, pos, &data);
    data_discard(&data);

    RETURN_LIST(list);
}

NATIVE_METHOD(replace) {
    int      pos,
             len;
    list_t * list;
    data_t   data;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(LIST)
    INIT_ARG2(INTEGER)

    len = list_length(LIST1);
    pos = INT2 - 1;

    if (pos < 0)
        THROW((range_id, "Position (%d) less than one", pos + 1))
    else if (pos > len - 1)
        THROW((range_id, "Position (%d) greater than length of list (%d)",
              pos + 1, len))

    data_dup(&data, &args[2]);
    list = list_dup(LIST1);
    CLEAN_STACK();
    anticipate_assignment();

    list = list_replace(list, pos, &data);
    data_discard(&data);
    
    RETURN_LIST(list);
}

NATIVE_METHOD(delete) {
    int      pos,
             len;
    list_t * list;

    INIT_2_ARGS(LIST, INTEGER)

    len = list_length(LIST1);
    pos = INT2 - 1;

    if (pos < 0)
        THROW((range_id, "Position (%d) less than one", pos + 1))
    else if (pos > len - 1)
        THROW((range_id, "Position (%d) greater than length of list (%d)",
              pos + 1, len))

    list = list_dup(LIST1);

    CLEAN_STACK();
    anticipate_assignment();

    RETURN_LIST(list_delete(list, pos));
}

NATIVE_METHOD(setadd) {
    list_t * list;
    data_t   data;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 2, "two")
    INIT_ARG1(LIST)

    data_dup(&data, &args[1]);
    list = list_dup(LIST1);

    CLEAN_STACK();
    anticipate_assignment();

    list = list_setadd(list, &data);
    data_discard(&data);

    RETURN_LIST(list);
}

NATIVE_METHOD(setremove) {
    list_t * list;
    data_t   data;
    DEF_args;
    
    INIT_ARGC(ARG_COUNT, 2, "two") 
    INIT_ARG1(LIST)

    data_dup(&data, &args[1]);
    list = list_dup(LIST1);
    
    CLEAN_STACK();
    anticipate_assignment();
    
    list = list_setremove(list, &data);
    data_discard(&data);
    
    RETURN_LIST(list);
}

NATIVE_METHOD(union) {
    list_t * list, * list2;

    INIT_2_ARGS(LIST, LIST)

    list = list_dup(LIST1);
    list2 = list_dup(LIST2);

    CLEAN_STACK();
    anticipate_assignment();

    list = list_union(list, list2);

    list_discard(list2);

    RETURN_LIST(list);
}

