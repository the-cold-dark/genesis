/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: ops/list.c
// ---
// List Manipulation functions
*/

#include "config.h"
#include "defs.h"
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "memory.h"
#include "util.h" /* fformat() */

void func_listgraft(void) {
    data_t * args, * d1, * d2;
    list_t * new, * l1, * l2;
    int pos, x;

    if (!func_init_3(&args, LIST, INTEGER, LIST))
        return;

    pos = args[1].u.val;
    l1 = args[0].u.list;
    l2 = args[2].u.list;

    if (pos-1 > list_length(l1) || pos < 1) {
        cthrow(range_id,
               "Position %D is outside of the range of the list.",
               &args[1]);
        return;
    } else if (pos == 1) {
        l2 = list_append(l2, l1);
        new = list_dup(l2);
    } else if (pos-1 == list_length(l1)) {
        l1 = list_append(l1, l2);
        new = list_dup(l1);
    } else {
        new = list_new(list_length(l1) + list_length(l2));
        for (x=2, d1=list_first(l1); d1; d1=list_next(l1, d1), x++) {
            new = list_add(new, d1);
            if (x==pos) {
                for (d2=list_first(l2); d2; d2=list_next(l2, d2))
                    new = list_add(new, d2);
            }
        }
    }

    pop(3);
    push_list(new);
    list_discard(new);
}

void func_listlen(void) {
    data_t *args;
    int len;

    /* Accept a list to take the length of. */
    if (!func_init_1(&args, LIST))
	return;

    /* Replace the argument with its length. */
    len = list_length(args[0].u.list);
    pop(1);
    push_int(len);
}

void func_sublist(void) {
    int num_args, start, span, list_len;
    data_t *args;

    /* Accept a list, an integer, and an optional integer. */
    if (!func_init_2_or_3(&args, &num_args, LIST, INTEGER, INTEGER))
	return;

    list_len = list_length(args[0].u.list);
    start = args[1].u.val - 1;
    span = (num_args == 3) ? args[2].u.val : list_len - start;

    /* Make sure range is in bounds. */
    if (start < 0) {
	cthrow(range_id, "Start (%d) less than one", start + 1);
    } else if (span < 0) {
	cthrow(range_id, "Sublist length (%d) less than zero", span);
    } else if (start + span > list_len) {
	cthrow(range_id, "Sublist extends to %d, past end of list (length %d)",
	      start + span, list_len);
    } else {
	/* Replace first argument with sublist, and pop other arguments. */
	anticipate_assignment();
	args[0].u.list = list_sublist(args[0].u.list, start, span);
	pop(num_args - 1);
    }
}

void func_insert(void) {
    int pos, list_len;
    data_t *args;

    /* Accept a list, an integer offset, and a data value of any type. */
    if (!func_init_3(&args, LIST, INTEGER, 0))
	return;

    pos = args[1].u.val - 1;
    list_len = list_length(args[0].u.list);

    if (pos < 0) {
	cthrow(range_id, "Position (%d) less than one", pos + 1);
    } else if (pos > list_len) {
	cthrow(range_id, "Position (%d) beyond end of list (length %d)",
	      pos + 1, list_len);
    } else {
	/* Modify the list and pop the offset and data. */
	anticipate_assignment();
	args[0].u.list = list_insert(args[0].u.list, pos, &args[2]);
	pop(2);
    }
}

void func_replace(void) {
    int pos, list_len;
    data_t *args;

    /* Accept a list, an integer offset, and a data value of any type. */
    if (!func_init_3(&args, LIST, INTEGER, 0))
	return;

    list_len = list_length(args[0].u.list);
    pos = args[1].u.val - 1;

    if (pos < 0) {
	cthrow(range_id, "Position (%d) less than one", pos + 1);
    } else if (pos > list_len - 1) {
	cthrow(range_id, "Position (%d) greater than length of list (%d)",
	      pos + 1, list_len);
    } else {
	/* Modify the list and pop the offset and data. */
	anticipate_assignment();
	args[0].u.list = list_replace(args[0].u.list, pos, &args[2]);
	pop(2);
    }
}

void func_delete(void) {
    int pos, list_len;
    data_t *args;

    /* Accept a list and an integer offset. */
    if (!func_init_2(&args, LIST, INTEGER))
	return;

    list_len = list_length(args[0].u.list);
    pos = args[1].u.val - 1;

    if (pos < 0) {
	cthrow(range_id, "Position (%d) less than one", pos + 1);
    } else if (pos > list_len - 1) {
	cthrow(range_id, "Position (%d) greater than length of list (%d)",
	      pos + 1, list_len);
    } else {
	/* Modify the list and pop the offset. */
	anticipate_assignment();
	args[0].u.list = list_delete(args[0].u.list, pos);
	pop(1);
    }
}

void func_setadd(void) {
    data_t *args;

    /* Accept a list and a data value of any type. */
    if (!func_init_2(&args, LIST, 0))
	return;

    /* Add args[1] to args[0] and pop args[1]. */
    anticipate_assignment();
    args[0].u.list = list_setadd(args[0].u.list, &args[1]);
    pop(1);
}

void func_setremove(void) {
    data_t *args;

    /* Accept a list and a data value of any type. */
    if (!func_init_2(&args, LIST, 0))
	return;

    /* Remove args[1] from args[0] and pop args[1]. */
    anticipate_assignment();
    args[0].u.list = list_setremove(args[0].u.list, &args[1]);
    pop(1);
}

void func_union(void) {
    data_t *args;

    /* Accept two lists. */
    if (!func_init_2(&args, LIST, LIST))
	return;

    /* Union args[1] into args[0] and pop args[1]. */
    anticipate_assignment();
    args[0].u.list = list_union(args[0].u.list, args[1].u.list);
    pop(1);
}

