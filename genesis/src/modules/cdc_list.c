/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define NATIVE_MODULE "$list"

#include "cdc.h"

NATIVE_METHOD(listlen) {
    Int len;

    INIT_1_ARG(LIST);

    len = list_length(LIST1);

    CLEAN_RETURN_INTEGER(len);
}

NATIVE_METHOD(sublist) {
    Int      start,
             span,
             len;
    cList * list;

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
    Int      pos,
             len;
    cList * list;
    cData   data;
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
    Int      pos,
             len;
    cList * list;
    cData   data;
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
    Int      pos,
             len;
    cList * list;

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
    cList * list;
    cData   data;
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
    cList * list;
    cData   data;
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
    cList * list, * list2;

    INIT_2_ARGS(LIST, LIST)

    list = list_dup(LIST1);
    list2 = list_dup(LIST2);

    CLEAN_STACK();
    anticipate_assignment();

    list = list_union(list, list2);

    list_discard(list2);

    RETURN_LIST(list);
}

NATIVE_METHOD(join) {
    Int      discard_sep=NO;
    cStr    * str, * sep;

    INIT_1_OR_2_ARGS(LIST, STRING)

    if (!LIST1->len) {
        str = string_new(0);
    } else {
        if (argc == 1) {
            sep = string_from_chars(" ", 1);
            discard_sep=YES;
        } else {    
            sep = STR2;
        }
        str = list_join(LIST1, sep);
        if (discard_sep)
            string_discard(sep);
    }
    
    CLEAN_RETURN_STRING(str);
}

