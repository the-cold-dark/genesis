/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: list.c
// ---
// Routines for list manipulation.
//
// This code is not ANSI-conformant, because it allocates memory at the end
// of List structure and references it with a one-element array.
*/

#include "config.h"
#include "defs.h"
#include "list.h"
#include "memory.h"
/*#include <assert.h>*/

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

list_t * list_prep(list_t *list, int start, int len) {
    list_t * cnew;
    int      i,
             resize,
             size;

    /* Figure out if we need to resize the list or move its contents.  Moving
     * contents takes precedence. */
    resize = (len - start) * 4 < list->size;
    resize = resize && list->size > STARTING_SIZE;
    resize = resize || (list->size < len);

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
        list = (list_t *) erealloc(list,
                                   sizeof(list_t) + (size * sizeof(data_t)));
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


list_t *list_new(int len) {
    list_t * cnew;
    cnew = (list_t *) emalloc(sizeof(list_t) + (len * sizeof(data_t)));
    cnew->len = 0;
    cnew->start = 0;
    cnew->size = len;
    cnew->refs = 1;
    return cnew;
}

list_t *list_dup(list_t *list) {
    list->refs++;
    return list;
}

int list_length(list_t *list) {
    return list->len;
}

data_t *list_first(list_t *list) {
    return (list->len) ? list->el + list->start : NULL;
}

data_t *list_next(list_t *list, data_t *d) {
    return (d < list->el + list->start + list->len - 1) ? d + 1 : NULL;
}

data_t *list_last(list_t *list) {
    return (list->len) ? list->el + list->start + list->len - 1 : NULL;
}

data_t *list_prev(list_t *list, data_t *d) {
    return (d > list->el + list->start) ? d - 1 : NULL;
}

data_t *list_elem(list_t *list, int i) {
    return list->el + list->start + i;
}

/* This is a horrible abstraction-breaking function.  Call it just after you
 * make a list with list_new(<spaces>).  Then fill in the data slots yourself.
 * Don't manipulate <list> until you're done. */
data_t * list_empty_spaces(list_t *list, int spaces) {
    list->len += spaces;
    return list->el + list->start + list->len - spaces;
}

int list_search(list_t *list, data_t *data) {
    data_t *d, *start, *end;

    start = list->el + list->start;
    end = start + list->len;
    for (d = start; d < end; d++) {
        if (data_cmp(data, d) == 0)
            return d - start;
    }
    return -1;
}

/* Effects: Returns 0 if the lists l1 and l2 are equivalent, or 1 if not. */
int list_cmp(list_t *l1, list_t *l2) {
    int i;

    /* They're obviously the same if they're the same list. */
    if (l1 == l2)
        return 0;

    /* Lists can only be equal if they're of the same length. */
    if (l1->len != l2->len)
        return 1;

    /* See if any elements differ. */
    for (i = 0; i < l1->len; i++) {
        if (data_cmp(&l1->el[l1->start + i], &l2->el[l2->start + i]) != 0)
            return 1;
    }

    /* No elements differ, so the lists are the same. */
    return 0;
}

/* Error-checking on pos is the job of the calling function. */
list_t *list_insert(list_t *list, int pos, data_t *elem) {
    list = list_prep(list, list->start, list->len + 1);
    pos += list->start;
    MEMMOVE(list->el + pos + 1, list->el + pos, list->len - 1 - pos);
    data_dup(&list->el[pos], elem);
    return list;
}

list_t *list_add(list_t *list, data_t *elem) {
    list = list_prep(list, list->start, list->len + 1);
    data_dup(&list->el[list->start + list->len - 1], elem);
    return list;
}

/* Error-checking on pos is the job of the calling function. */
list_t *list_replace(list_t *list, int pos, data_t *elem) {
    /* list_prep needed here only for multiply referenced lists */
    if (list->refs > 1)
      list = list_prep(list, list->start, list->len);
    pos += list->start;
    data_discard(&list->el[pos]);
    data_dup(&list->el[pos], elem);
    return list;
}

/* Error-checking on pos is the job of the calling function. */
list_t *list_delete(list_t *list, int pos) {
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
list_t *list_delete_element(list_t *list, data_t *elem) {
    return list_delete(list, list_search(list, elem));
}

list_t *list_append(list_t *list1, list_t *list2) {
    int i;
    data_t *p, *q;

    list1 = list_prep(list1, list1->start, list1->len + list2->len);
    p = list1->el + list1->start + list1->len - list2->len;
    q = list2->el + list2->start;
    for (i = 0; i < list2->len; i++)
        data_dup(&p[i], &q[i]);
    return list1;
}

list_t *list_reverse(list_t *list) {
    data_t *d, tmp;
    int i;

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

list_t *list_setadd(list_t *list, data_t *d) {
    if (list_search(list, d) != -1)
        return list;
    return list_add(list, d);
}

list_t *list_setremove(list_t *list, data_t *d) {
    int pos = list_search(list, d);
    if (pos == -1)
        return list;
    return list_delete(list, pos);
}

list_t *list_union(list_t *list1, list_t *list2) {
    data_t *start, *end, *d;

    /* Simplistic O(len1 * len2) implementation for now.  Later, use lengths to
     * decide whether to use a O(len1 + len2) hash table algorithm. */
    start = list2->el + list2->start;
    end = start + list2->len;
    for (d = start; d < end; d++) {
        if (list_search(list1, d) == -1)
            list1 = list_add(list1, d);
    }
    return list1;
}

list_t *list_sublist(list_t *list, int start, int len) {
    return list_prep(list, list->start + start, len);
}

/* Warning: do not discard a list before initializing its data elements. */
void list_discard(list_t *list) {
    int i;

    if (!--list->refs) {
        for (i = list->start; i < list->start + list->len; i++)
            data_discard(&list->el[i]);
        efree(list);
    }
}

