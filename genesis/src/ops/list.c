/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include "operators.h"
#include "execute.h"
#include "util.h" /* fformat() */

COLDC_FUNC(listgraft) {
    cData * args, * d1, * d2;
    cList * new, * l1, * l2;
    Int pos, x;

    if (!func_init_3(&args, LIST, INTEGER, LIST))
        return;

    pos = INT2 - 1;
    l1 = LIST1;
    l2 = LIST3;

    if (pos > list_length(l1) || pos < 0)
        THROW((range_id, "Position %D is outside of the range of the list.",
               &args[1]))

    l1 = list_dup(l1);
    l2 = list_dup(l2);
    anticipate_assignment();
    pop(3);

    if (pos == 0) {
        l2 = list_append(l2, l1);
        push_list(l2);
    } else if (pos == list_length(l1)) {
        l1 = list_append(l1, l2);
        push_list(l1);
    } else {
        new = list_new(list_length(l1) + list_length(l2));
        pos++;
        for (x=2, d1=list_first(l1); d1; d1=list_next(l1, d1), x++) {
            new = list_add(new, d1);
            if (x==pos) {
                for (d2=list_first(l2); d2; d2=list_next(l2, d2))
                    new = list_add(new, d2);
            }
        }
        push_list(new);
        list_discard(new);
    }
    list_discard(l1);
    list_discard(l2);
}

COLDC_FUNC(listlen) {
    cData *args;
    Int len;

    /* Accept a list to take the length of. */
    if (!func_init_1(&args, LIST))
	return;

    /* Replace the argument with its length. */
    len = list_length(args[0].u.list);
    pop(1);
    push_int(len);
}

COLDC_FUNC(sublist) {
    Int num_args, start, span, list_len;
    cData *args;

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

COLDC_FUNC(insert) {
    Int pos, list_len;
    cData *args;

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

COLDC_FUNC(replace) {
    Int pos, list_len;
    cData *args;

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

COLDC_FUNC(delete) {
    Int pos, list_len;
    cData *args;

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

COLDC_FUNC(setadd) {
    cData *args;

    /* Accept a list and a data value of any type. */
    if (!func_init_2(&args, LIST, 0))
	return;

    /* Add args[1] to args[0] and pop args[1]. */
    anticipate_assignment();
    args[0].u.list = list_setadd(args[0].u.list, &args[1]);
    pop(1);
}

COLDC_FUNC(setremove) {
    cData *args;

    /* Accept a list and a data value of any type. */
    if (!func_init_2(&args, LIST, 0))
	return;

    /* Remove args[1] from args[0] and pop args[1]. */
    anticipate_assignment();
    args[0].u.list = list_setremove(args[0].u.list, &args[1]);
    pop(1);
}

COLDC_FUNC(union) {
    cData *args;

    /* Accept two lists. */
    if (!func_init_2(&args, LIST, LIST))
	return;

    /* Union args[1] into args[0] and pop args[1]. */
    anticipate_assignment();
    args[0].u.list = list_union(args[0].u.list, args[1].u.list);
    pop(1);
}

COLDC_FUNC(join) {
    cData * args;
    Int      argc, discard_sep=NO;
    cStr    * str, * sep;

    if (!func_init_1_or_2(&args, &argc, LIST, STRING))
        return;

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

    pop(argc);
    push_string(str);
    string_discard(str);
}

COLDC_FUNC(listidx) {
    int origin;
    int r; 
    
    INIT_2_OR_3_ARGS(LIST, ANY_TYPE, INTEGER);

    if (argc == 3)  
        origin = INT3;
    else
        origin = 1;
    
    if (!LIST1->len) {
        pop(argc);
        push_int(0);
        return;
    }

    if ((r = list_index(LIST1, &args[1], origin)) == F_FAILURE)
        THROW((range_id, "Origin is beyond the range of the list."))
    
    pop(argc); 
    push_int(r);
}   

