/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_buffer.c
// ---
// Buffer manipulation module
*/

#include "config.h"
#include "y.tab.h"
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"

void op_buffer_len(void) {
    data_t *args;
    int len;

    if (!func_init_1(&args, BUFFER))
	return;
    len = buffer_len(args[0].u.buffer);
    pop(1);
    push_int(len);
}

void op_buffer_retrieve(void) {
    data_t *args;
    int c, pos;

    if (!func_init_2(&args, BUFFER, INTEGER))
	return;
    pos = args[1].u.val - 1;
    if (pos < 0) {
	cthrow(range_id, "Position (%d) is less than one.", pos + 1);
    } else if (pos >= buffer_len(args[0].u.buffer)) {
	cthrow(range_id, "Position (%d) is greater than buffer length (%d).",
	      pos + 1, buffer_len(args[0].u.buffer));
    } else {
	c = buffer_retrieve(args[0].u.buffer, pos);
	pop(2);
	push_int(c);
    }
}

void op_buffer_append(void) {
    data_t *args;

    if (!func_init_2(&args, BUFFER, BUFFER))
	return;
    args[0].u.buffer = buffer_append(args[0].u.buffer, args[1].u.buffer);
    pop(1);
}

void op_buffer_replace(void) {
    data_t *args;
    int pos;

    if (!func_init_3(&args, BUFFER, INTEGER, INTEGER))
	return;
    pos = args[1].u.val - 1;
    if (pos < 0) {
	cthrow(range_id, "Position (%d) is less than one.", pos + 1);
	return;
    } else if (pos >= buffer_len(args[0].u.buffer)) {
	cthrow(range_id, "Position (%d) is greater than buffer length (%d).",
	      pos + 1, buffer_len(args[0].u.buffer));
	return;
    }
    args[0].u.buffer = buffer_replace(args[0].u.buffer, pos, args[2].u.val);
    pop(2);
}

void op_buffer_add(void) {
    data_t *args;

    if (!func_init_2(&args, BUFFER, INTEGER))
	return;
    args[0].u.buffer = buffer_add(args[0].u.buffer, args[1].u.val);
    pop(1);
}

void op_buffer_truncate(void) {
    data_t *args;
    int pos;

    if (!func_init_2(&args, BUFFER, INTEGER))
	return;
    pos = args[1].u.val;
    if (pos < 0) {
	cthrow(range_id, "Position (%d) is less than zero.", pos);
	return;
    } else if (pos > buffer_len(args[0].u.buffer)) {
	cthrow(range_id, "Position (%d) is greater than buffer length (%d).",
	      pos, buffer_len(args[0].u.buffer));
	return;
    }
    args[0].u.buffer = buffer_resize(args[0].u.buffer, pos);
    pop(1);
}

void op_buffer_tail(void) {
    data_t *args;
    int pos;

    if (!func_init_2(&args, BUFFER, INTEGER))
	return;
    pos = args[1].u.val;
    if (pos < 1) {
        cthrow(range_id, "Position (%d) is less than one.", pos);
        return;
    } else if (pos > buffer_len(args[0].u.buffer)) {
        cthrow(range_id, "Position (%d) is greater than buffer length (%d).",
               pos, buffer_len(args[0].u.buffer));
        return;
    }
    args[0].u.buffer = buffer_tail(args[0].u.buffer, pos);
    pop(1);
}

void op_buffer_to_string(void) {
    data_t *args;
    string_t * str;

    if (!func_init_1(&args, BUFFER))
	return;
    str = buffer_to_string(args[0].u.buffer);
    pop(1);
    push_string(str);
    string_discard(str);
}

void op_buffer_to_strings(void) {
    data_t *args;
    int num_args;
    list_t *list;
    Buffer *sep;

    if (!func_init_1_or_2(&args, &num_args, BUFFER, BUFFER))
	return;
    sep = (num_args == 2) ? args[1].u.buffer : NULL;
    list = buffer_to_strings(args[0].u.buffer, sep);
    pop(num_args);
    push_list(list);
    list_discard(list);
}

void op_buffer_from_string(void) {
    data_t *args;
    Buffer *buf;

    if (!func_init_1(&args, STRING))
	return;
    buf = buffer_from_string(args[0].u.str);
    pop(1);
    push_buffer(buf);
    buffer_discard(buf);
}


void op_buffer_from_strings(void) {
    data_t *args, *d;
    int num_args, i;
    Buffer *buf, *sep;
    list_t *list;

    if (!func_init_1_or_2(&args, &num_args, LIST, BUFFER))
	return;

    list = args[0].u.list;
    sep = (num_args == 2) ? args[1].u.buffer : NULL;

    for (d = list_first(list), i=0; d; d = list_next(list, d),i++) {
	if (d->type != STRING) {
	    cthrow(type_id, "List element %d (%D) not a string.", i + 1, d);
	    return;
	}
    }

    buf = buffer_from_strings(list, sep);
    pop(num_args);
    push_buffer(buf);
    buffer_discard(buf);
}

