/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define NATIVE_MODULE "$buffer"

#include "cdc.h"

NATIVE_METHOD(buflen) {
    Int val;

    INIT_1_ARG(BUFFER);

    val = buffer_len(BUF1);

    CLEAN_RETURN_INTEGER(val);
}

NATIVE_METHOD(buf_replace) {
    cBuf * buf;
    Int pos, ch;

    INIT_3_ARGS(BUFFER, INTEGER, INTEGER);

    pos = INT2 - 1;

    if (pos < 0)
        THROW((range_id, "Position (%d) is less than one.", pos + 1))
    else if (pos >= buffer_len(BUF1))
	THROW((range_id, "Position (%d) is greater than buffer length (%d).",
	      pos + 1, buffer_len(BUF1)))

    ch = INT3;
    buf = buffer_dup(BUF1);

    anticipate_assignment();

    CLEAN_RETURN_BUFFER(buffer_replace(buf, pos, ch));
}

NATIVE_METHOD(subbuf) {
    Int      start,
             len,
             blen;
    cBuf * buf;

    INIT_2_OR_3_ARGS(BUFFER, INTEGER, INTEGER);

    blen = BUF1->len;
    start = INT2 - 1;

    len = (argc == 3) ? INT3 : blen - start;

    if (start < 0)
        THROW((range_id, "Start (%d) is less than one.", start + 1))
    else if (len < 0)
        THROW((range_id, "Length (%d) is less than zero.", len))
    else if (start + len > blen)
        THROW((range_id,
              "The subrange extends to %d, past the end of the buffer (%d).",
              start + len, blen))

    buf = buffer_dup(BUF1);

    anticipate_assignment();

    CLEAN_RETURN_BUFFER(buffer_subrange(buf, start, len));
}

NATIVE_METHOD(buf_to_str) {
    cBuf * buf;

    INIT_1_ARG(BUFFER);

    buf = buffer_dup(BUF1);

    CLEAN_RETURN_STRING(buf_to_string(buf));
}

NATIVE_METHOD(buf_to_strings) {
    cList * list;
    cBuf * sep;

    INIT_1_OR_2_ARGS(BUFFER, BUFFER);

    sep = (argc == 2) ? BUF2 : NULL;
    list = buf_to_strings(BUF1, sep);

    CLEAN_RETURN_LIST(list);
}

NATIVE_METHOD(str_to_buf) {
    cBuf * buf;

    INIT_1_ARG(STRING);

    anticipate_assignment();
    buf = buffer_from_string(STR1);

    CLEAN_RETURN_BUFFER(buf);
}


NATIVE_METHOD(strings_to_buf) {
    cData * d;
    Int      i;
    cBuf * sep,
           * buf;
    cList * list;

    INIT_1_OR_2_ARGS(LIST, BUFFER);

    list = LIST1;
    sep = (argc == 2) ? BUF2 : NULL;

    for (d = list_first(list), i=0; d; d = list_next(list, d),i++) {
	if (d->type != STRING)
            THROW((type_id, "List element %d (%D) not a string.", i + 1, d))
    }

    buf = buffer_from_strings(list, sep);

    CLEAN_RETURN_BUFFER(buf);
}

