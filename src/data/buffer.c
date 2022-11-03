/* -*- -*-
// Full copyright information is available in the file ../doc/CREDITS
//
// Routines for ColdC buffer manipulation.
*/

#include "defs.h"

#include <ctype.h>
#include "util.h"
#include "macros.h"

#define BUFFER_OVERHEAD     (sizeof(cBuf))

cBuf *buffer_new(Int size_needed) {
    cBuf *buf;

    size_needed = ROUND_UP(size_needed + BUFFER_OVERHEAD, BUFFER_DATA_INCREMENT);
    buf = (cBuf*)emalloc(size_needed);
    buf->len = 0;
    buf->size = size_needed - BUFFER_OVERHEAD;
    buf->refs = 1;
    return buf;
}

cBuf *buffer_dup(cBuf *buf) {
    buf->refs++;
    return buf;
}

void buffer_discard(cBuf *buf) {
    buf->refs--;
    if (!buf->refs)
        efree(buf);
}

cBuf *buffer_append(cBuf *buf1, const cBuf *buf2) {
    if (!buf2->len)
        return buf1;
    buf1 = buffer_prep(buf1, buf1->len + buf2->len);
    MEMCPY(buf1->s + buf1->len, buf2->s, buf2->len);
    buf1->len += buf2->len;
    return buf1;
}

cBuf * buffer_append_uchars_single_ref(cBuf * buf, const unsigned char * new, Int new_len) {
    Int new_size = buf->len + new_len;

    if (buf->size < new_size) {
        /* Resize the buffer */
        new_size = ROUND_UP(new_size + BUFFER_OVERHEAD, BUFFER_DATA_INCREMENT);
        buf = (cBuf*)erealloc(buf, new_size);
        buf->size = new_size - BUFFER_OVERHEAD;
    }

    MEMCPY(buf->s + buf->len, new, new_len);
    buf->len += new_len;
    return buf;
}

cBuf * buffer_append_uchars(cBuf * buf1, const unsigned char * new, Int new_len) {
    if (!new_len)
        return buf1;
    buf1 = buffer_prep(buf1, buf1->len + new_len);
    MEMCPY(buf1->s + buf1->len, new, new_len);
    buf1->len += new_len;
    return buf1;
}

Int buffer_retrieve(const cBuf *buf, Int pos) {
    return buf->s[pos];
}

cBuf *buffer_replace(cBuf *buf, Int pos, uInt c) {
    if (buf->s[pos] == c)
        return buf;
    buf = buffer_prep(buf, buf->len);
    buf->s[pos] = OCTET_VALUE(c);
    return buf;
}

cBuf *buffer_add(cBuf *buf, uInt c) {
    buf = buffer_prep(buf, buf->len + 1);
    buf->s[buf->len] = OCTET_VALUE(c);
    buf->len++;
    return buf;
}

cBuf *buffer_resize(cBuf *buf, Int len) {
    /* This is currently only called to shrink the buffer. */
    /* Calling it to enlarge the buffer could be dangerous, as
       as it leaves some uninitialized memory in the buffer */

    if (len == buf->len)
        return buf;
    buf = buffer_prep(buf, len);
    buf = (cBuf*)erealloc(buf, len + BUFFER_OVERHEAD);
    buf->size = len;
    buf->len = len;
    return buf;
}


/* REQUIRES char *s and unsigned char *q are defined */
cStr * buf_to_string(const cBuf * buf) {
    cStr * str, * out;
    const unsigned char * string_start, *p, *q;
    char * s;
    size_t len;
    const char sepchar = '\n';
    const int seplen = 1;

    out = string_new(buf->len);
    string_start = p = buf->s;
    while (p + seplen <= buf->s + buf->len) {
        p = (unsigned char *) memchr(p, sepchar, (buf->s + buf->len) - p);
        if (!p)
            break;
        str = string_new(p - string_start);
        s = str->s;
        for (q = string_start; q < p; q++) {
            if (ISPRINT(*q))
                *s++ = *q;
            else if (*q == '\t')
                *s++ = ' ';
        }
        *s = 0;
        str->len = s - str->s;
        out = string_add(out, str);
        out = string_add_chars(out, "\\n", 2);
        string_discard(str);
        string_start = p = p + seplen;
    }

    if ((len = (buf->s + buf->len) - string_start)) {
        str = string_new(len);
        s = str->s;
        for (q = string_start; len--; q++) {
            if (ISPRINT(*q))
                *s++ = *q;
            else if (*q == '\t')
                *s++ = ' ';
        }
        *s = '\0';
        str->len = s - str->s;
        out = string_add(out, str);
        string_discard(str);

    }

    return out;
}

/* If sep (separator buffer) is NULL, separate by newlines. */
cList *buf_to_strings(const cBuf *buf, const cBuf *sep)
{
    cData d;
    cStr *str;
    cList *result;
    unsigned char sepchar;
    const unsigned char *string_start, *p, *q;
    char *s;
    Int seplen;
    cBuf *end;

    sepchar = (sep) ? *sep->s : '\n';
    seplen = (sep) ? sep->len : 1;
    result = list_new(0);
    string_start = p = buf->s;
    d.type = STRING;
    while (p + seplen <= buf->s + buf->len) {
        /* Look for sepchar staring from p. */
        p = (unsigned char *)memchr(p, sepchar,
                                    (buf->s + buf->len) - (p + seplen - 1));
        if (!p)
            break;

        /* Keep going if we don't match all of the separator. */
        if (sep && MEMCMP(p + 1, sep->s + 1, seplen - 1) != 0) {
            p++;
            continue;
        }

        /* We found a separator.  Copy the printable characters in the
         * intervening text into a string. */
        str = string_new(p - string_start);
        s = str->s;
        for (q = string_start; q < p; q++) {
            if (ISPRINT(*q))
                *s++ = *q;
            else if (*q == '\t')
                *s++ = ' ';
        }
        *s = '\0';
        str->len = s - str->s;

        d.u.str = str;
        result = list_add(result, &d);
        string_discard(str);

        string_start = p = p + seplen;
    }

    /* Add the remainder characters to the list as a buffer. */
    end = buffer_new(buf->s + buf->len - string_start);
    MEMCPY(end->s, string_start, buf->s + buf->len - string_start);
    end->len = buf->s + buf->len - string_start;
    d.type = BUFFER;
    d.u.buffer = end;
    result = list_add(result, &d);
    buffer_discard(end);

    return result;
}

cBuf *buffer_from_string(const cStr * string) {
    cBuf * buf;
    Int    new, str_len;

    str_len = string_length(string);
    buf = buffer_new(str_len);
    buf->len = str_len;
    new = parse_strcpy((char *) buf->s,
                       string_chars(string),
                       str_len);

    if (str_len - new)
        buf = buffer_resize(buf, new);

    return buf;
}

cBuf *buffer_from_strings(cList * string_list, const cBuf * sep) {
    cData * string_data;
    cBuf *buf;
    Int num_strings, i, len, pos;
    unsigned char *s;

    string_data = list_first(string_list);
    num_strings = list_length(string_list);

    /* Find length of finished buffer. */
    len = 0;
    for (i = 0; i < num_strings; i++)
        len += string_length(string_data[i].u.str) + ((sep) ? sep->len : 2);

    /* Make a buffer and copy the strings into it. */
    buf = buffer_new(len);
    buf->len = len;
    pos = 0;
    for (i = 0; i < num_strings; i++) {
        s = (unsigned char *) string_chars(string_data[i].u.str);
        len = string_length(string_data[i].u.str);
        MEMCPY(buf->s + pos, s, len);
        pos += len;
        if (sep) {
            MEMCPY(buf->s + pos, sep->s, sep->len);
            pos += sep->len;
        } else {
            buf->s[pos++] = '\r';
            buf->s[pos++] = '\n';
        }
    }

    return buf;
}

cBuf * buffer_subrange(cBuf * buf, Int start, Int len) {
    cBuf * cnew = buffer_new(len);

    MEMCPY(cnew->s, buf->s + start, (len > buf->len ? buf->len : len));
    cnew->len = len;
    buffer_discard(buf);

    return cnew;
}

cBuf * buffer_bufsub(cBuf * buf, cBuf * old, cBuf * new) {
    cBuf *cnew;
    Int lb, lo, ln, p, q;

    lb = buf->len;
    lo = old->len;
    ln = new->len;

    if (((lo == ln) && (memcmp(old->s, new->s, lo) == 0)) ||
        (lo > lb) ||
        (lo == 0) ||
        (lb == 0))
        return buf;

    p = q = 1;
    cnew = buffer_new(buf->len);
    while ((p <= buf->len) &&
           (q = buffer_index(buf, old->s, lo, p)) &&
           (q != -1)) {
        cnew = buffer_append_uchars_single_ref(cnew, buf->s + p - 1, q - p);
        cnew = buffer_append_uchars_single_ref(cnew, new->s, ln);
        p = q + lo;
    }
    cnew = buffer_append_uchars_single_ref(cnew, buf->s + p - 1, lb - p + 1);

    buffer_discard(buf);
    buffer_discard(old);
    buffer_discard(new);
    return cnew;
}

cBuf *buffer_prep(cBuf *buf, Int new_size) {
    cBuf *cnew;

    if (buf->refs != 1) {
        /* Make a new buffer with the same contents as the old one. */
        buf->refs--;
        cnew = buffer_new(new_size);
        MEMCPY(cnew->s, buf->s, buf->len);
        cnew->len = buf->len;
        return cnew;
    } else if (buf->size < new_size) {
        /* Resize the buffer */
        new_size = ROUND_UP(new_size + BUFFER_OVERHEAD, BUFFER_DATA_INCREMENT);
        buf = (cBuf*)erealloc(buf, new_size);
        buf->size = new_size - BUFFER_OVERHEAD;
        return buf;
    } else {
        return buf;
    }
}

static
int buf_rindexs(const unsigned char * buf, int len, const unsigned char * sub, int slen, int origin){
    const unsigned char * s;

    if (origin < slen)
        origin = slen;

    len -= origin;

    if (len < 0)
        return 0;

    s = &buf[len];

    while (len-- >= 0) {
        if (*s == *sub) {
            if (!MEMCMP(s, sub, slen))
                return (s - buf) + 1;
        }
        s--;
    }

    return 0;
}

static int buf_rindexc(const unsigned char * buf, int len, unsigned char sub, int origin) {
    const unsigned char * s;

    len -= origin;

    if (len < 0)
        return 0;

    s = &buf[len];

    while (len-- >= 0) {
        if (*s == sub)
            return (s - buf) + 1;
        s--;
    }

    return 0;
}

/*
// returns 1..$ if item is found, 0 if it is not or -1 if an error is thrown
*/

int buffer_index(const cBuf * buf, const unsigned char * ss, int slen, int origin) {
    int     len;
    const unsigned char * s,
                * p;
    bool    reverse = false;

    s = buf->s;
    len = buf->len;

    if (origin < 0) {
        reverse = true;
        origin = -origin;
    }

    if (origin > len || !origin)
        return F_FAILURE;

    if (reverse) {
        if (slen == 1)
            return buf_rindexc(s, len, *ss, origin);
        return buf_rindexs(s, len, ss, slen, origin);
    } else {
        int xlen = len;

        origin--;
        xlen -= origin;

        if (xlen < slen)
            return 0;

        p = s + origin;

        p = (unsigned char *) memchr(p, *ss, xlen);

        if (slen == 1)
            return p ? ((p - s) + 1) : 0;

        while (p && (p+slen <= s+len)) {
            if (MEMCMP(p, ss, slen) == 0)
                return (p - s) + 1;
            xlen = len - ((p - s) + 1);
            p = (unsigned char *) memchr(p+1, *ss, xlen);
        }
    }
    return 0;
}

