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

static void merge_lists (cData *l, cData *key,
			 Int start1, Int end1,
			 Int start2, Int end2,
			 cData *l_out, cData *key_out)
{
    Int i,j,k;

    i=start1;
    j=start2;
    k=start1;

    while (i<=end1 && j<=end2)
	if (data_cmp(key+j,key+i)>=0)
	    key_out[k]=key[i], l_out[k++]=l[i++];
	else
	    key_out[k]=key[j], l_out[k++]=l[j++];
    while (i<=end1)
	key_out[k]=key[i], l_out[k++]=l[i++];
    while (j<=end2)
	key_out[k]=key[j], l_out[k++]=l[j++];
    memcpy (l+start1, l_out+start1, sizeof(cData)*(k-start1));
    memcpy (key+start1, key_out+start1, sizeof(cData)*(k-start1));
}

static void merge_sort (cData *l, cData *key,
			cData *l1, cData *key1,
			Int start, Int end)
{

    Int mid;

    if (start==end)
	return;

    mid=(start+end)/2;
    merge_sort (l, key, l1, key1, start, mid);
    merge_sort (l, key, l1, key1, mid+1, end);
    merge_lists (l, key, start, mid, mid+1, end, l1, key1);
}

NATIVE_METHOD(sort) {
    cData *d1, *d2, *key1, *key2;
    Int n, i;
    cList *data, *keys;
    cList *out;

    INIT_1_OR_2_ARGS(LIST, LIST);
    data=LIST1;
    if (argc==1)
	keys=data;
    else
	keys=LIST2;

    n=list_length(data);
    if (!(list_length(keys)==n)) {
	THROW((range_id, "Key and data lists are not of the same length"));
    }

    if (!n) {
	out=list_dup(data);
	CLEAN_RETURN_LIST(out);
    }

    d1=emalloc(sizeof(cData)*n);
    d2=emalloc(sizeof(cData)*n);
    key1=emalloc(sizeof(cData)*n);
    key2=emalloc(sizeof(cData)*n);

    for (i=0; i<n; i++) {
	data_dup(d1+i, list_elem(data, i));
	data_dup(key1+i, list_elem(keys, i));
    }
    merge_sort (d1, key1, d2, key2, 0, n-1);

    out=list_new(n);
    out->len=n;
    for (i=0; i<n; i++) {
        *list_elem(out, i)=d1[i]; /* We already did data_dup */
	data_discard(key1+i);
    }

    efree(d1);
    efree(d2);
    efree(key1);
    efree(key2);

    CLEAN_RETURN_LIST(out);
}
