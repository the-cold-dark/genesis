/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include "cdc_pcode.h"

COLDC_FUNC(bufgraft) {
    cData * args;  
    register cBuf * new, * b1, * b2;
    Int pos;

    if (!func_init_3(&args, BUFFER, INTEGER, BUFFER))
        return;

    pos = INT2 - 1;
    b1  = BUF1;
    b2  = BUF3;

    if (pos > buffer_len(b1) || pos < 0)
        THROW((range_id, "Position %D is outside of the range of the buffer.",
               &args[1]))

    b1  = buffer_dup(b1);
    b2  = buffer_dup(b2);
    anticipate_assignment();
    pop(3);

    if (pos == 0) {
        b2 = buffer_append(b2, b1);
        push_buffer(b2);
    } else if (pos == buffer_len(b1)) {
        b1 = buffer_append(b1, b2);
        push_buffer(b1);
    } else {
        new = buffer_new(b1->len + b2->len);
        MEMCPY(new->s, b1->s, pos);
        MEMCPY(new->s + pos, b2->s, b2->len);
        MEMCPY(new->s + pos + b2->len, b1->s + pos, b1->len - pos + 1);
        new->len = b1->len + b2->len;
        push_buffer(new);
        buffer_discard(new);
    }
    buffer_discard(b1);
    buffer_discard(b2);
}

COLDC_FUNC(buflen) {
    cData * args;
    Int len;

    if (!func_init_1(&args, BUFFER))
        return;

    len = buffer_len(_BUF(ARG1));

    pop(1);
    push_int(len);
}

COLDC_FUNC(buf_replace) {
    cData * args;
    Int pos;

    if (!func_init_3(&args, BUFFER, INTEGER, INTEGER))
        return;

    pos = _INT(ARG2) - 1;
    if (pos < 0)
	THROW((range_id, "Position (%d) is less than one.", pos + 1))
    else if (pos >= buffer_len(_BUF(ARG1)))
	THROW((range_id, "Position (%d) is greater than buffer length (%d).",
	      pos + 1, buffer_len(_BUF(ARG1))))

    _BUF(ARG1) = buffer_replace(_BUF(ARG1), pos, _INT(ARG3));

    pop(2);
}

COLDC_FUNC(subbuf) {
    cData *args;
    Int start, len, nargs, blen;

    if (!func_init_2_or_3(&args, &nargs, BUFFER, INTEGER, INTEGER))
	return;

    blen = args[0].u.buffer->len;
    start = args[1].u.val - 1;
    len = (nargs == 3) ? args[2].u.val : blen - start;

    if (start < 0)
        THROW((range_id, "Start (%d) is less than one.", start + 1))
    else if (len < 0)
        THROW((range_id, "Length (%d) is less than zero.", len))
    else if (start + len > blen)
        THROW((range_id,
              "The subrange extends to %d, past the end of the buffer (%d).",
              start + len, blen))

    anticipate_assignment();
    args[0].u.buffer = buffer_subrange(args[0].u.buffer, start, len);
    pop(nargs - 1);
}

COLDC_FUNC(buf_to_str) {
    cData *args;
    cStr * str;

    if (!func_init_1(&args, BUFFER))
	return;

    str = buf_to_string(args[0].u.buffer);

    pop(1);
    push_string(str);
    string_discard(str);
}

COLDC_FUNC(buf_to_strings) {
    cData *args;
    Int num_args;
    cList *list;
    cBuf *sep;

    if (!func_init_1_or_2(&args, &num_args, BUFFER, BUFFER))
	return;

    sep = (num_args == 2) ? args[1].u.buffer : NULL;
    list = buf_to_strings(args[0].u.buffer, sep);

    pop(num_args);
    push_list(list);
    list_discard(list);
}

COLDC_FUNC(str_to_buf) {
    cData *args;
    cBuf *buf;

    if (!func_init_1(&args, STRING))
	return;
    buf = buffer_from_string(args[0].u.str);
    pop(1);
    push_buffer(buf);
    buffer_discard(buf);
}


COLDC_FUNC(strings_to_buf) {
    cData *args, *d;
    Int num_args, i;
    cBuf *buf, *sep;
    cList *list;

    if (!func_init_1_or_2(&args, &num_args, LIST, BUFFER))
	return;

    list = args[0].u.list;
    sep = (num_args == 2) ? args[1].u.buffer : NULL;

    for (d = list_first(list), i=0; d; d = list_next(list, d),i++) {
	if (d->type != STRING)
            THROW((type_id, "List element %d (%D) not a string.", i + 1, d))
    }

    buf = buffer_from_strings(list, sep);

    pop(num_args);
    push_buffer(buf);
    buffer_discard(buf);
}

COLDC_FUNC(bufidx) {
    int     origin;
    int     r;
    uChar   c;
    int     clen;
    uChar * cp;
    
    INIT_2_OR_3_ARGS(BUFFER, ANY_TYPE, INTEGER);
    
    if (argc == 3)
        origin = INT3;
    else
        origin = 1; 

    if (args[1].type == INTEGER) {
        c = (uChar) args[1].u.val;
        cp = &c;
        clen = 1;
    } else if (args[1].type == BUFFER) {
        cp = BUF2->s;
        clen = BUF2->len;
    } else
        THROW((type_id, "Second argument must be a buffer or integer."))

    if (!buffer_len(BUF1)) {
        pop(argc);
        push_int(0);
        return;
    }
    
    if ((r = buffer_index(BUF1, cp, clen, origin)) == F_FAILURE)
        THROW((range_id, "Origin is beyond the range of the buffer."))

    pop(argc); 
    push_int(r);
}

