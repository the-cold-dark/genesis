/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Routines for list manipulation.
//
// This code is not ANSI-conformant, because it allocates memory at the end
// of List structure and references it with a one-element array.
*/

#include "defs.h"
#include "quickhash.h"

/* Note that we number list elements [0..(len - 1)] internally, while the
 * user sees list elements as numbered [1..len]. */

/* We use MALLOC_DELTA to keep our blocks about 32 bytes less than a power of
 * two.  We also have to account for the size of a List (16 bytes) which gets
 * added in before we allocate.  This works if a Data is sixteen bytes. */
  
#define MALLOC_DELTA    3
#define STARTING_SIZE   (16 - MALLOC_DELTA)

/* Input to this routine should be a list you want to modify, a start, and a
 * length.  The start gives the offset from list->el at which you start being
 * interested in data; the length is the amount of data there will be in the
 * list after that point after you finish modifying it.
 *
 * The return value of this routine is a list whose contents can be freely
 * modified, containing at least the information you claimed was interesting.
 * list->start will be set to the beginning of the interesting data; list->len
 * will be set to len, even though this will make some data invalid if
 * len > list->len upon input.  Also, the returned string may not be null-
 * terminated.
 *
 * If start is increased or len is decreased by this function, and list->refs
 * is 1, the uninteresting data will be discarded by this function.
 *
 * In general, modifying start and len is the responsibility of this routine;
 * modifying the contents is the responsibility of the calling routine. */

cList * list_prep(cList *list, Int start, Int len) {
    cList * cnew;
    Int      i,
             resize,
             size;

    /* Figure out if we need to resize the list or move its contents.  Moving
     * contents takes precedence. */
#if DISABLED
    resize = (len - start) * 4 < list->size;
    resize = resize && list->size > STARTING_SIZE;
    resize = resize || (list->size < len);
#endif
    resize = list->size < len + start;


    /* Move the list contents into a new list. */
    if ((list->refs > 1) || (resize && start > 0)) {
        cnew = list_new(len);
        cnew->len = len;
        len = (list->len < len) ? list->len : len;
        for (i = 0; i < len; i++)
            data_dup(&cnew->el[i], &list->el[start + i]);
        list_discard(list);
        return cnew;
    }

    /* Resize the list.  We can assume that list->start == start == 0. */
    else if (resize) {
        for (; list->len > len; list->len--)
            data_discard(&list->el[list->len - 1]);
        list->len = len;
        size = len;
        list = (cList *) erealloc(list,
                                   sizeof(cList) + (size * sizeof(cData)));
        list->size = size;
        return list;
    }

    else {
        for (; list->start < start; list->start++, list->len--)
            data_discard(&list->el[list->start]);
        for (; list->len > len; list->len--)
            data_discard(&list->el[list->start + list->len - 1]);
        list->start = start;
        list->len = len;
        return list;
    }
}


cList *list_new(Int len) {
    cList * cnew;
    cnew = (cList *) emalloc(sizeof(cList) + (len * sizeof(cData)));
    cnew->len = 0;
    cnew->start = 0;
    cnew->size = len;
    cnew->refs = 1;
    return cnew;
}

cList *list_dup(cList *list) {
    list->refs++;
    return list;
}

Int list_length(cList *list) {
    return list->len;
}

cData *list_first(cList *list) {
    return (list->len) ? list->el + list->start : NULL;
}

cData *list_next(cList *list, cData *d) {
    return (d < list->el + list->start + list->len - 1) ? d + 1 : NULL;
}

cData *list_last(cList *list) {
    return (list->len) ? list->el + list->start + list->len - 1 : NULL;
}

cData *list_prev(cList *list, cData *d) {
    return (d > list->el + list->start) ? d - 1 : NULL;
}

cData *list_elem(cList *list, Int i) {
    return list->el + list->start + i;
}

/* This is a horrible abstraction-breaking function.  Call it just after you
 * make a list with list_new(<spaces>).  Then fill in the data slots yourself.
 * Don't manipulate <list> until you're done. */
cData * list_empty_spaces(cList *list, Int spaces) {
    list->len += spaces;
    return list->el + list->start + list->len - spaces;
}

Int list_search(cList *list, cData *data) {
    cData *d, *start, *end;

    start = list->el + list->start;
    end = start + list->len;
    for (d = start; d < end; d++) {
        if (data_cmp(data, d) == 0)
            return d - start;
    }
    return -1;
}

/* Effects: Returns 0 if the lists l1 and l2 are equivalent, or 1 if not. */
Int list_cmp(cList *l1, cList *l2) {
    Int i, k;

    /* They're obviously the same if they're the same list. */
    if (l1 == l2)
        return 0;

    /* Lists can only be equal if they're of the same length. */
    if (l1->len != l2->len)
        return 1;

    /* See if any elements differ. */
    for (i = 0; i < l1->len; i++) {
        if ((k=data_cmp(&l1->el[l1->start + i], &l2->el[l2->start + i])) != 0)
            return k;
    }

    /* No elements differ, so the lists are the same. */
    return 0;
}

/* Error-checking on pos is the job of the calling function. */
cList *list_insert(cList *list, Int pos, cData *elem) {
    list = list_prep(list, list->start, list->len + 1);
    pos += list->start;
    MEMMOVE(list->el + pos + 1, list->el + pos, list->len - 1 - pos);
    data_dup(&list->el[pos], elem);
    return list;
}

cList *list_add(cList *list, cData *elem) {
    list = list_prep(list, list->start, list->len + 1);
    data_dup(&list->el[list->start + list->len - 1], elem);
    return list;
}

/* Error-checking on pos is the job of the calling function. */
cList *list_replace(cList *list, Int pos, cData *elem) {
    /* list_prep needed here only for multiply referenced lists */
    if (list->refs > 1)
      list = list_prep(list, list->start, list->len);
    pos += list->start;
    data_discard(&list->el[pos]);
    data_dup(&list->el[pos], elem);
    return list;
}

/* Error-checking on pos is the job of the calling function. */
cList *list_delete(cList *list, Int pos) {
    /* Special-case deletion of last element. */
    if (pos == list->len - 1)
        return list_prep(list, list->start, list->len - 1);

    /* list_prep needed here only for multiply referenced lists */
    if (list->refs > 1)
        list = list_prep(list, list->start, list->len);

    pos += list->start;
    data_discard(&list->el[pos]);
    MEMMOVE(list->el + pos, list->el + pos + 1, list->len - pos);
    list->len--;

    /* list_prep needed here only if list has shrunk */
    if (((list->len - list->start) * 4 < list->size)
        && (list->size > STARTING_SIZE))
        list = list_prep(list, list->start, list->len);

    return list;
}

/* This routine will crash if elem is not in list. */
cList *list_delete_element(cList *list, cData *elem) {
    return list_delete(list, list_search(list, elem));
}

cList *list_append(cList *list1, cList *list2) {
    Int i;
    cData *p, *q;

    list1 = list_prep(list1, list1->start, list1->len + list2->len);
    p = list1->el + list1->start + list1->len - list2->len;
    q = list2->el + list2->start;
    for (i = 0; i < list2->len; i++)
        data_dup(&p[i], &q[i]);
    return list1;
}

cList *list_reverse(cList *list) {
    cData *d, tmp;
    Int i;

    /* list_prep needed here only for multiply referenced lists */
    if (list->refs > 1)
        list = list_prep(list, list->start, list->len);

    d = list->el + list->start;
    for (i = 0; i < list->len / 2; i++) {
        tmp = d[i];
        d[i] = d[list->len - i - 1];
        d[list->len - i - 1] = tmp;
    }
    return list;
}

cList *list_setadd(cList *list, cData *d) {
    if (list_search(list, d) != -1)
        return list;
    return list_add(list, d);
}

cList *list_setremove(cList *list, cData *d) {
    Int pos = list_search(list, d);
    if (pos == -1)
        return list;
    return list_delete(list, pos);
}

cList *list_union(cList *list1, cList *list2) {
    cData *start, *end, *d;

    start = list2->el + list2->start;
    end = start + list2->len;
    if (list1->len + list2->len < 12) {
        for (d = start; d < end; d++) {
            if (list_search(list1, d) == -1)
                list1 = list_add(list1, d);
        }
    } else {
        Hash * tmp;
        
        tmp = hash_new_with(list1);
        list_discard(list1);
        for (d = start; d < end; d++) {
            tmp = hash_add(tmp, d);
        }
        list1 = list_dup(tmp->keys);
        hash_discard(tmp);
    }
    return list1;
}

cList *list_sublist(cList *list, Int start, Int len) {
    return list_prep(list, list->start + start, len);
}

/* Warning: do not discard a list before initializing its data elements. */
void list_discard(cList *list) {
    Int i;

    if (!--list->refs) {
        for (i = list->start; i < list->start + list->len; i++)
            data_discard(&list->el[i]);
        efree(list);
    }
}

/* 'list' must ALWAYS have at least one element, it does not check */
#define ADD_TOSTR() \
    switch (d->type) { \
        case STRING: \
            s = string_add(s, d->u.str); \
            break; \
        case SYMBOL: \
            sp = ident_name(d->u.symbol); \
            s = string_add_chars(s, sp, strlen(sp)); \
            break; \
        default: \
            s = data_add_literal_to_str(s, d, DF_WITH_OBJNAMES); \
            break; \
    }

cStr * list_join(cList * list, cStr * sep) {
    Int size;
    cData * d;
    cStr * s;
    char * sp;
    
    /* figure up the size of the resulting string */
    size = sep->len * (list->len - 1);
    for (d=list_first(list); d; d = list_next(list, d)) {
        /* just guess on its resulting size, magic numbers, whee */
        if (d->type != STRING)
            size += 5;
        else
            size += d->u.str->len;
    }

    s = string_new(size);

    d = list_first(list);
    ADD_TOSTR() 
    for (d=list_next(list, d); d; d = list_next(list, d)) {
        s = string_add(s, sep);
        ADD_TOSTR()
    }   
    
    return s;
}

int list_index(cList * list, cData * search, int origin) {
    int     len;
    Bool    reverse = NO;
    cData * d,
          * start,
          * end;

    len = list_length(list);
    
    if (origin < 0) {
        reverse = YES;
        origin = -origin;
    }

    if (origin > len || !origin)
        return F_FAILURE;

    if (origin > len)
        return 0;

    origin--;
    start = list->el + list->start;
    end = start + list->len;

    if (reverse) {
        end -= (origin + 1);
        for (d = end; d >= start; d--) {
            if (data_cmp(search, d) == 0)
                return (d - start) + 1;
        }
    } else {
        for (d = start + origin; d < end; d++) {
            if (data_cmp(search, d) == 0)
                return (d - start) + 1;
        }
    }
    return 0;
}

