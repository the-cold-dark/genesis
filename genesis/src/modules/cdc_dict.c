/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define NATIVE_MODULE "$dictionary"

#include "cdc.h"

NATIVE_METHOD(dict_values) {
    cList * list;

    INIT_1_ARG(DICT);

    list = dict_values(DICT1);

    CLEAN_RETURN_LIST(list);
}

NATIVE_METHOD(dict_keys) {
    cList * list;

    INIT_1_ARG(DICT);

    list = dict_keys(DICT1);

    CLEAN_RETURN_LIST(list);
}

/* ugh; what we do to keep from copying */
NATIVE_METHOD(dict_add) {
    cDict * dict;
    cData   arg1, arg2;
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
    cData arg1;
    cDict * dict;
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
    Int val;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 2, "two");
    INIT_ARG1(DICT);

    val = dict_contains(DICT1, &args[1]);

    CLEAN_RETURN_INTEGER(val);
}

NATIVE_METHOD(dict_add_elem) {
    cDict  * dict;
    cData    listd, d, key, value;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(DICT);

    if (dict_find(DICT1, &args[1], &listd) == keynf_id) {
        listd.type = LIST;
        listd.u.list = list_new(0);
    } else if (listd.type != LIST) {
        cthrow(type_id, "Value for %D (%D) is not a list.", &args[0], &listd);
        data_discard(&listd);
        RETURN_FALSE;
    }

    /*
    // set the list to zero to remove the reference to the list, then
    // dup the dictionary so we can clear the stack and anticipate the
    // assignmenent, all to keep references at their most minimal so we
    // do not copy if we do not need to.
    //
    // also dup the key and the value being inserted.
    */

    d.type = INTEGER;
    d.u.val = 0;
    dict = dict_add(DICT1, &args[1], &d);
    dict = dict_dup(dict);
    data_dup(&key, &args[1]);
    data_dup(&value, &args[2]);

    /* clean the stack and anticipate the assignment */
    CLEAN_STACK();
    anticipate_assignment();

    /* Add the element to the list. */
    listd.u.list = list_add(listd.u.list, &value);
    dict = dict_add(dict, &key, &listd);

    data_discard(&key);
    data_discard(&value);
    data_discard(&listd);

    RETURN_DICT(dict);
}

NATIVE_METHOD(dict_del_elem) {
    cDict  * dict;
    cData    dlist, d, elem, key;
    cList  * list;
    Int       pos;
    DEF_args;

    INIT_ARGC(ARG_COUNT, 3, "three");
    INIT_ARG1(DICT);

    if (dict_find(DICT1, &args[1], &dlist) == keynf_id)
        THROW((keynf_id, "Key (%D) is not in the dictionary.", &args[1]))
    else if (dlist.type != LIST) {
        cthrow(type_id, "Value for %D (%D) is not a list.", &args[0], &dlist);
        data_discard(&dlist);
        RETURN_FALSE;
    }

    dict = dict_dup(DICT1);
    data_dup(&key,  &args[1]);
    data_dup(&elem, &args[2]);

    list = dlist.u.list;

    /* clean the stack and anticipate the assignment */
    CLEAN_STACK();
    anticipate_assignment();

    /* find the element in the list */
    pos = list_search(list, &elem);

    /* we are finished with 'elem' */
    data_discard(&elem); 

    /* the element is not in the list, simply return the dictionary */
    if (pos == -1) {
        data_discard(&dlist); 
        data_discard(&key); 
        RETURN_DICT(dict);
    }

    /* if the list length is one, then simply delete the dict element */
    if (list_length(list) == 1) {
        dict = dict_del(dict, &key);
        data_discard(&dlist);
        data_discard(&key);
        RETURN_DICT(dict);
    }

    /*
    // Temorarily set the dictionary's value for the key to zero, to
    // remove the reference to the list.
    */
    d.type = INTEGER;
    d.u.val = 0;
    dict = dict_add(dict, &key, &d);

    /* Remove elem from the list */
    dlist.u.list = list_delete(list, pos);

    /* Set the new list as the value for the key in the dictionary */
    dict = dict_add(dict, &key, &dlist);

    /* clean up and return */
    data_discard(&dlist);
    data_discard(&key);

    RETURN_DICT(dict);
}

NATIVE_METHOD(dict_union) {
    cDict * dict1, * dict2;

    INIT_2_ARGS(DICT, DICT)

    dict1 = dict_dup(DICT1);
    dict2 = dict_dup(DICT2);

    CLEAN_STACK();
    anticipate_assignment();

    RETURN_DICT(dict_union(dict1, dict2));
}

