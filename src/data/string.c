/*
// Full copyright information is available in the file ../doc/CREDITS
//
// This code is not ANSI-conformant, because it allocates memory at the end
// of String structure and references it with a one-element array.
*/

#include "defs.h"

#include <string.h>
#include "dbpack.h"
#include "util.h"

/* Note that we number string elements [0..(len - 1)] internally, while the
 * user sees string elements as numbered [1..len]. */

/* Many implementations of malloc() deal best with blocks eight or sixteen
 * bytes less than a power of two.  MALLOC_DELTA and STRING_STARTING_SIZE take
 * this into account.  We start with a string MALLOC_DELTA bytes less than
 * a power of two.  When we enlarge a string, we double it and add MALLOC_DELTA
 * so that we're still MALLOC_DELTA less than a power of two.  When we
 * allocate, we add in sizeof(cStr), leaving us 32 bytes short of a power of
 * two, as desired. */

#define MALLOC_DELTA        (sizeof(cStr) + 32)
#define STARTING_SIZE        (128 - MALLOC_DELTA)

cStr *string_new(Int size_needed) {
    cStr *cnew;
    Int size;

    /* plus one for NULL */
    size = size_needed + 1;
    cnew = (cStr *) emalloc(sizeof(cStr) + sizeof(char) * size);
    cnew->start = 0;
    cnew->len = 0;
    cnew->size = size;
    cnew->refs = 1;
    cnew->reg = NULL;
    *cnew->s = 0;
    return cnew;
}

cStr *string_from_chars(char *s, Int len) {
    cStr *cnew = string_new(len);

    MEMCPY(cnew->s, s, len);
    cnew->s[len] = '\0';
    cnew->len = len;
    return cnew;
}

cStr *string_of_char(Int c, Int len) {
    cStr *cnew = string_new(len);

    memset(cnew->s, c, len);
    cnew->s[len] = 0;
    cnew->len = len;
    return cnew;
}

cStr *string_dup(cStr *str) {
    str->refs++;
    return str;
}

cBuf *string_pack(cBuf *buf, cStr *str) {
    if (str) {
        buf = write_long(buf, str->len);
        buf = buffer_append_uchars_single_ref(buf, (uChar *)(str->s + str->start), str->len);
    } else {
        buf = write_long(buf, -1);
    }
    return buf;
}

cStr *string_unpack(cBuf *buf, Long *buf_pos) {
    cStr *str;
    Int len;

    len = read_long(buf, buf_pos);
    if (len == -1) {
        /*fprintf(stderr, "string_unpack: NULL @%d\n", ftell(fp));*/
        return NULL;
    }
    str = string_new(len);
    str->len = len;
    MEMCPY(str->s, &(buf->s[*buf_pos]), len);
    (*buf_pos) += len;
    str->s[len] = 0;
    return str;
}

Int string_packed_size(cStr *str, int memory_size) {
    int size = 0;

    if (memory_size)
    {
        if (!str)
            return 0;

        size += sizeof(cStr);
        /* size += hrm.. regexp is rare and a little complicated */
    }
    else
    {
        if (!str)
            return size_long(-1, 0);

        size += size_long(str->len, 0);
    }

    size += str->len * sizeof(char);

    return size;
}

Int string_cmp(cStr *str1, cStr *str2) {
    return strcmp(str1->s + str1->start, str2->s + str2->start);
}

cStr *string_fread(cStr *str, Int len, FILE *fp) {
    str = string_prep(str, str->start, str->len + len);
    fread(str->s + str->start + str->len - len, sizeof(char), len, fp);
    return str;
}

cStr *string_add(cStr *str1, cStr *str2) {
    str1 = string_prep(str1, str1->start, str1->len + str2->len);
    MEMCPY(str1->s + str1->start + str1->len - str2->len,
           str2->s + str2->start, str2->len);
    str1->s[str1->start + str1->len] = 0;
    return str1;
}

/* calling this with len == 0 can be a problem */
cStr *string_add_chars(cStr *str, char *s, Int len) {
    str = string_prep(str, str->start, str->len + len);
    MEMCPY(str->s + str->start + str->len - len, s, len);
    str->s[str->start + str->len] = 0;
    return str;
}

cStr *string_addc(cStr *str, Int c) {
    str = string_prep(str, str->start, str->len + 1);
    str->s[str->start + str->len - 1] = c;
    str->s[str->start + str->len] = 0;
    return str;
}

cStr *string_add_padding(cStr *str, char *filler, Int len, Int padding) {
    str = string_prep(str, str->start, str->len + padding);

    if (len == 1) {
        /* Optimize this case using memset(). */
        memset(str->s + str->start + str->len - padding, *filler, padding);
        return str;
    }

    while (padding > len) {
        MEMCPY(str->s + str->start + str->len - padding, filler, len);
        padding -= len;
    }
    MEMCPY(str->s + str->start + str->len - padding, filler, padding);
    return str;
}

cStr *string_truncate(cStr *str, Int len) {
    str = string_prep(str, str->start, len);
    str->s[str->start + len] = 0;
    return str;
}

cStr *string_substring(cStr *str, Int start, Int len) {
    str = string_prep(str, str->start + start, len);
    str->s[str->start + str->len] = 0;
    return str;
}

cStr * string_uppercase(cStr *str) {
    char *s, *end;

    str = string_prep(str, str->start, str->len);
    s = string_chars(str);
    end = s + str->len;
    for (; s < end; s++)
        *s = (char) UCASE(*s);
    return str;
}

cStr * string_lowercase(cStr *str) {
    char *s, *end;

    str = string_prep(str, str->start, str->len);
    s = string_chars(str);
    end = s + str->len;
    for (; s < end; s++)
        *s = (char) LCASE(*s);
    return str;
}

/* Compile str's regexp, if it's not already compiled.  If there is an error,
 * it will be placed in regexp_error, and the returned regexp will be NULL. */
regexp *string_regexp(cStr *str) {
    if (!str->reg)
        str->reg = gen_regcomp(str->s + str->start);
    return str->reg;
}

void string_discard(cStr *str) {
    if (!--str->refs) {
        if (str->reg)
            efree(str->reg);
        efree(str);
    }
}

cStr * string_parse(char **sptr) {
    cStr * str;
    char * p;
    char * s;
    Int len;

#ifndef ONLY_PARSE_TEXTDB
    str = string_new(0);
#endif

    s = *sptr;
    while (s && *s != '"') /* search until end of string or closing " */
    {
        p = strpbrk(s, "\"\\"); /* search for escape char or closing " */
        if (p)
        {
#ifndef ONLY_PARSE_TEXTDB
            str = string_add_chars(str, s, p-s);
#endif
            /* if its the escape char, look at the next to see if its */
            /* really escaping anything.  ColdC escaping isn't just like C */
            if (*p == '\\')
            {
                if (*(p+1) == '"' || *(p+1) == '\\')
                    p++;
#ifndef ONLY_PARSE_TEXTDB
                str = string_addc(str, *p);
#endif
                p++;
            }
            s = p;
        }
        else
        {
            /* This should never occur, since the string should always be    */
            /* terminated by a ".  So, add everything left and setup to end  */
            /* the loop */
            len = strlen(s);
#ifndef ONLY_PARSE_TEXTDB
            str = string_add_chars(str, s, len);
#endif
            s += len;
        }
    }
    *sptr = *s ? s+1 : s;

    /* give them the string */
    return str;
}

cStr *string_add_unparsed(cStr *str, char *s, Int len) {
    Int i;

    str = string_addc(str, '"');

    /* Add characters to string, escaping quotes and backslashes. */
    for (;;) {
        for (i = 0; i < len && s[i] != '"' && s[i] != '\\'; i++);
        str = string_add_chars(str, s, i);
        if (i < len) {
            if (s[i] == '"' || /* it is a slash, by default */
                (s[i + 1] == '\\' ||
                 s[i + 1] == '"' ||
                 s[i + 1] == '\0'))
                str = string_addc(str, '\\');
            str = string_addc(str, s[i]);
            s += i + 1;
            len -= i + 1;
        } else {
            break;
        }
    }

    return string_addc(str, '"');
}

char *gen_regerror(char *msg) {
    static char *regexp_error;

    if (msg)
        regexp_error = msg;
    return regexp_error;
}

/*
// -------------------------------------------------------------
// index() and company
*/

static int str_rindexs(char * str, int len, char * sub, int slen, int origin){
    register char * s;

    if (origin < slen)
        origin = slen;

    len -= origin;

    if (len < 0)
        return 0;

    s = &str[len];

    while (len-- >= 0) {
        if (*s == *sub) {
            if (!strncmp(s, sub, slen))
                return (s - str) + 1;
        }
        s--;
    }

    return 0;
}

static int str_rindexc(char * str, int len, char sub, int origin) {
    register char * s;

    len -= origin;

    if (len < 0)
        return 0;

    s = &str[len];

    while (len-- >= 0) {
        if (*s == sub)
            return (s - str) + 1;
        s--;
    }

    return 0;
}

/*
// returns 1..$ if item is found, 0 if it is not or -1 if an error is thrown
*/

int string_index(cStr * str, cStr * sub, int origin) {
    int    len,
           slen;
    char * s,
         * p,
         * ss;
    Bool   reverse = false;

    s = string_chars(str);
    len = string_length(str);
    ss = string_chars(sub);
    slen = string_length(sub);

    if (origin < 0) {
        reverse = true;
        origin = -origin;
    }

    if (origin > len || !origin)
        return F_FAILURE;

    if (reverse) {
        if (slen == 1)
            return str_rindexc(s, len, *ss, origin);
        return str_rindexs(s, len, ss, slen, origin);
    } else if (origin > 0) {
        origin--;
        if (len - origin < slen)
            return 0;
        p = s + origin;
        if ((p = strcstr(p, ss)))
            return (p - s)+1;
    }
    return 0;
}

/*
// Input to this routine should be a string you want to modify, a start, and a
// length.  The start gives the offset from str->s at which you start being
// interested in characters; the length is the amount of characters there will
// be in the string past that point after you finish modifying it.
//
// The return value of this routine is a string whose contents can be freely
// modified, containing at least the information you claimed was interesting.
// str->start will be set to the beginning of the interesting characters;
// str->len will be set to len, even though this will make some characters
// invalid if len > str->len upon input.  Also, the returned string may not be
// null-terminated.
//
// In general, modifying start and len is the responsibility of this routine;
// modifying the contents is the responsibility of the calling routine.
*/
cStr * string_prep(cStr *str, Int start, Int len) {
    cStr *cnew;
    Int need_to_move, need_to_resize, size;

    /* Figure out if we need to resize the string or move its contents.  Moving
     * contents takes precedence. */
    need_to_resize = str->size <= len + start;
    need_to_move = (str->refs > 1) || (need_to_resize && start > 0);


    if (need_to_move) {
        /* Move the string's contents into a new string. */
        cnew = string_new(len);
        MEMCPY(cnew->s, str->s + start, (len > str->len) ? str->len : len);
        cnew->s[len] = '\0';
        cnew->len = len;
        string_discard(str);
        return cnew;
    } else if (need_to_resize) {
        /* Resize the string.  We can assume that string->start == start == 0 */
        str->len = len;
        size = len + 1; /* plus one for NULL */
        str = (cStr *)erealloc(str, sizeof(cStr)+(size * sizeof(char)));
        str->s[start+len] = '\0';
        str->size = size;
        return str;
    } else {
        if (str->reg) {
            efree(str->reg);
            str->reg = NULL;
        }
        str->start = start;
        str->len = len;
        str->s[start+len] = '\0';
        return str;
    }
}

