/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: ops/buffer.c
// ---
// Buffer manipulation functions
*/

#define NATIVE_MODULE "$buffer"

#include "config.h"
#include "defs.h"
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"

NATIVE_METHOD(bufgraft) {
    INIT_NO_ARGS()

    RETURN_INTEGER(0)
}

NATIVE_METHOD(buflen) {
    INIT_1_ARG(BUFFER)

    RETURN_INTEGER(buffer_len(_BUF(ARG1)))
}

NATIVE_METHOD(buf_replace) {
    int pos;

    INIT_3_ARGS(BUFFER, INTEGER, INTEGER)

    pos = _INT(ARG2) - 1;

    if (pos < 0)
        THROW((range_id, "Position (%d) is less than one.", pos + 1))
    else if (pos >= buffer_len(_BUF(ARG1)))
	THROW((range_id, "Position (%d) is greater than buffer length (%d).",
	      pos + 1, buffer_len(_BUF(ARG1))))

    RETURN_BUFFER(buffer_replace(_BUF(ARG1), pos, _INT(ARG3)));
}

NATIVE_METHOD(subbuf) {
    int      start,
             len,
             blen;

    INIT_2_OR_3_ARGS(BUFFER, INTEGER, INTEGER);

    blen = _BUF(ARG1)->len;
    start = _INT(ARG2) - 1;

    len = (argc == 3) ? _INT(ARG3) : blen - start;

    if (start < 0)
        THROW((range_id, "Start (%d) is less than one.", start + 1))
    else if (len < 0)
        THROW((range_id, "Length (%d) is less than zero.", len))
    else if (start + len > blen)
        THROW((range_id,
              "The subrange extends to %d, past the end of the buffer (%d).",
              start + len, blen))

    RETURN_BUFFER(buffer_subrange(_BUF(ARG1), start, len));
}

NATIVE_METHOD(buf_to_str) {
    INIT_1_ARG(BUFFER);

    RETURN_STRING(buffer_to_string(_BUF(ARG1)));
}

NATIVE_METHOD(buf_to_strings) {
    list_t *list;
    Buffer *sep;

    INIT_1_OR_2_ARGS(BUFFER, BUFFER);

    sep = (argc == 2) ? _BUF(ARG2) : NULL;
    list = buffer_to_strings(_BUF(ARG1), sep);

    RETURN_LIST(list);
}

NATIVE_METHOD(str_to_buf) {
    INIT_1_ARG(STRING);

    RETURN_BUFFER(buffer_from_string(_STR(ARG1)));
}


NATIVE_METHOD(strings_to_buf) {
    data_t * d;
    int      i;
    Buffer * sep;
    list_t * list;

    INIT_1_OR_2_ARGS(LIST, BUFFER);

    list = args[0].u.list;
    sep = (argc == 2) ? args[1].u.buffer : NULL;

    for (d = list_first(list), i=0; d; d = list_next(list, d),i++) {
	if (d->type != STRING)
            THROW((type_id, "List element %d (%D) not a string.", i + 1, d));
    }

    RETURN_BUFFER(buffer_from_strings(list, sep));
}

