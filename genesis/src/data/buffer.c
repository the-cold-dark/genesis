/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Routines for ColdC buffer manipulation.
*/

#include "defs.h"

#include <ctype.h>
#include "util.h"

#define BUFALLOC(len)		(cBuf *)emalloc(sizeof(cBuf) + (len) - 1)
#define BUFREALLOC(buf, len)	(cBuf *)erealloc(buf, sizeof(cBuf) + (len) - 1)

cBuf *buffer_new(Int len) {
    cBuf *buf;

    buf = BUFALLOC(len);
    buf->len = len;
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

cBuf *buffer_append(cBuf *buf1, cBuf *buf2) {
    if (!buf2->len)
	return buf1;
    buf1 = buffer_prep(buf1);
    buf1 = BUFREALLOC(buf1, buf1->len + buf2->len);
    MEMCPY(buf1->s + buf1->len, buf2->s, buf2->len);
    buf1->len += buf2->len;
    return buf1;
}

Int buffer_retrieve(cBuf *buf, Int pos) {
    return buf->s[pos];
}

cBuf *buffer_replace(cBuf *buf, Int pos, uInt c) {
    if (buf->s[pos] == c)
	return buf;
    buf = buffer_prep(buf);
    buf->s[pos] = OCTET_VALUE(c);
    return buf;
}

cBuf *buffer_add(cBuf *buf, uInt c) {
    buf = buffer_prep(buf);
    buf = BUFREALLOC(buf, buf->len + 1);
    buf->s[buf->len] = OCTET_VALUE(c);
    buf->len++;
    return buf;
}

cBuf *buffer_resize(cBuf *buf, Int len) {
    if (len == buf->len)
	return buf;
    buf = buffer_prep(buf);
    buf = BUFREALLOC(buf, len);
    buf->len = len;
    return buf;
}

cStr * buf_to_string(cBuf * buf) {
    cStr * str, * out;
    unsigned char * string_start, *p, *q;
    char * s;
    size_t len;

#define SEPCHAR '\n'
#define SEPLEN 1

    out = string_new(0);
    string_start = p = buf->s;
    while (p + SEPLEN <= buf->s + buf->len) {
        p = (unsigned char *) memchr(p, SEPCHAR, (buf->s + buf->len) - p);
        if (!p)
            break;
        str = string_new(p - string_start);
        s = str->s;
        for (q = string_start; q < p; q++) {
            if (ISPRINT(*q))
                *s++ = *q;
        }
        *s = 0;
        str->len = s - str->s;
        out = string_add(out, str);
        out = string_add_chars(out, "\\n", 2);
        string_discard(str);
        string_start = p = p + SEPLEN;
    }

    len = (buf->s + buf->len) - string_start;
    s = (char *) tmalloc(sizeof(char) + len);
    MEMCPY(s, string_start, len);
    out = string_add_chars(out, s, len);
    tfree(s, len);

#undef SEPCHAR
#undef SEPLEN

    return out;
}

/* If sep (separator buffer) is NULL, separate by newlines. */
cList *buf_to_strings(cBuf *buf, cBuf *sep)
{
    cData d;
    cStr *str;
    cList *result;
    unsigned char sepchar, *string_start;
    register unsigned char *p, *q;
    register char *s;
    Int seplen;
    cBuf *end;

    sepchar = (sep) ? *sep->s : '\n';
    seplen = (sep) ? sep->len : 1;
    result = list_new(0);
    string_start = p = buf->s;
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
	}
	*s = 0;
	str->len = s - str->s;

	d.type = STRING;
	d.u.str = str;
	result = list_add(result, &d);
	string_discard(str);

	string_start = p = p + seplen;
    }

    /* Add the remainder characters to the list as a buffer. */
    end = buffer_new(buf->s + buf->len - string_start);
    MEMCPY(end->s, string_start, buf->s + buf->len - string_start);
    d.type = BUFFER;
    d.u.buffer = end;
    result = list_add(result, &d);
    buffer_discard(end);

    return result;
}

cBuf *buffer_from_string(cStr * string) {
    cBuf * buf;
    Int      new;

    buf = buffer_new(string_length(string));
    new = parse_strcpy((char *) buf->s,
                       string_chars(string),
                       string_length(string));

    if (string_length(string) - new)
        buf = buffer_resize(buf, new);

    return buf;
}

cBuf *buffer_from_strings(cList * string_list, cBuf * sep) {
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

cBuf *buffer_prep(cBuf *buf) {
    cBuf *cnew;

    if (buf->refs == 1)
	return buf;

    /* Make a new buffer with the same contents as the old one. */
    buf->refs--;
    cnew = buffer_new(buf->len);
    MEMCPY(cnew->s, buf->s, buf->len);
    return cnew;
}

