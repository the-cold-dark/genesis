/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: string.c
// ---
// string.c: String-handling routines.
// This code is not ANSI-conformant, because it allocates memory at the end
// of String structure and references it with a one-element array.
*/

#include "config.h"
#include "defs.h"

#include <string.h>
#include "cdc_string.h"
#include "memory.h"
#include "dbpack.h"
#include "util.h"

/* Note that we number string elements [0..(len - 1)] internally, while the
 * user sees string elements as numbered [1..len]. */

/* Many implementations of malloc() deal best with blocks eight or sixteen
 * bytes less than a power of two.  MALLOC_DELTA and STRING_STARTING_SIZE take
 * this into account.  We start with a string MALLOC_DELTA bytes less than
 * a power of two.  When we enlarge a string, we double it and add MALLOC_DELTA
 * so that we're still MALLOC_DELTA less than a power of two.  When we
 * allocate, we add in sizeof(string_t), leaving us 32 bytes short of a power of
 * two, as desired. */

#define MALLOC_DELTA	(sizeof(string_t) + 32)
#define STARTING_SIZE	(128 - MALLOC_DELTA)

INTERNAL string_t *prepare_to_modify(string_t *str, int start, int len);

string_t *string_new(int size_needed) {
    string_t *cnew;
    int size;

#if DISABLED
    size = STARTING_SIZE;
    while (size < size_needed)
	size = size * 2 + MALLOC_DELTA;
#else
    size = size_needed + 1;
#endif
    cnew = (string_t *) emalloc(sizeof(string_t) + sizeof(char) * size);
    cnew->start = 0;
    cnew->len = 0;
    cnew->size = size;
    cnew->refs = 1;
    cnew->reg = NULL;
    *cnew->s = 0;
    return cnew;
}

string_t *string_from_chars(char *s, int len) {
    string_t *cnew = string_new(len);

    MEMCPY(cnew->s, s, len);
    cnew->s[len] = 0;
    cnew->len = len;
    return cnew;
}

string_t *string_of_char(int c, int len) {
    string_t *cnew = string_new(len);

    memset(cnew->s, c, len);
    cnew->s[len] = 0;
    cnew->len = len;
    return cnew;
}

string_t *string_dup(string_t *str) {
    str->refs++;
    return str;
}

void string_pack(string_t *str, FILE *fp) {
    if (str) {
	write_long(str->len, fp);
	fwrite(str->s + str->start, sizeof(char), str->len, fp);
    } else {
	write_long(-1, fp);
    }
}

string_t *string_unpack(FILE *fp) {
    string_t *str;
    int len;
    int result;

    len = read_long(fp);
    if (len == -1) {
      /*fprintf(stderr, "string_unpack: NULL @%d\n", ftell(fp));*/
      return NULL;
    }
    str = string_new(len);
    str->len = len;
    result = fread(str->s, sizeof(char), len, fp);
    str->s[len] = 0;
    return str;
}

int string_packed_size(string_t *str) {
    if (str)
	return size_long(str->len) + str->len * sizeof(char);
    else
	return size_long(-1);
}

int string_cmp(string_t *str1, string_t *str2) {
    return strcmp(str1->s + str1->start, str2->s + str2->start);
}

string_t *string_fread(string_t *str, int len, FILE *fp) {
    str = prepare_to_modify(str, str->start, str->len + len);
    fread(str->s + str->start + str->len - len, sizeof(char), len, fp);
    return str;
}

string_t *string_add(string_t *str1, string_t *str2) {
    str1 = prepare_to_modify(str1, str1->start, str1->len + str2->len);
    MEMCPY(str1->s + str1->start + str1->len - str2->len,
	   str2->s + str2->start, str2->len);
    str1->s[str1->start + str1->len] = 0;
    return str1;
}

/* calling this with len == 0 can be a problem */
string_t *string_add_chars(string_t *str, char *s, int len) {
    str = prepare_to_modify(str, str->start, str->len + len);
    MEMCPY(str->s + str->start + str->len - len, s, len);
    /*str->s[str->len + str->start + len] = 0;*/
    str->s[str->start + str->len] = 0;
    return str;
}

string_t *string_addc(string_t *str, int c) {
    str = prepare_to_modify(str, str->start, str->len + 1);
    str->s[str->start + str->len - 1] = c;
    str->s[str->start + str->len] = 0;
    return str;
}

string_t *string_add_padding(string_t *str, char *filler, int len, int padding) {
    str = prepare_to_modify(str, str->start, str->len + padding);

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

string_t *string_truncate(string_t *str, int len) {
    str = prepare_to_modify(str, str->start, len);
    str->s[str->start + len] = 0;
    return str;
}

string_t *string_substring(string_t *str, int start, int len) {
    str = prepare_to_modify(str, str->start + start, len);
    str->s[str->start + str->len] = 0;
    return str;
}

string_t * string_uppercase(string_t *str) {
    char *s, *start, *end;

    start = str->s + str->start;
    end = start + str->len;
    for (s = start; s < end; s++)
	*s = UCASE(*s);
    return str;
}

string_t * string_lowercase(string_t *str) {
    char *s, *start, *end;

    start = str->s + str->start;
    end = start + str->len;
    for (s = start; s < end; s++)
	*s = LCASE(*s);
    return str;
}

/* Compile str's regexp, if it's not already compiled.  If there is an error,
 * it will be placed in regexp_error, and the returned regexp will be NULL. */
regexp *string_regexp(string_t *str) {
    if (!str->reg)
	str->reg = regcomp(str->s + str->start);
    return str->reg;
}

void string_discard(string_t *str) {
    if (!--str->refs) {
	if (str->reg)
	    efree(str->reg);
	efree(str);
    }
}

string_t *string_parse(char **sptr) {
    string_t *str;
    char *s = *sptr, *p;

    str = string_new(0);
    s++;
    /* escape quote and backslash */
    while (1) {
	for (p = s; *p && *p != '"' && *p != '\\'; p++);
	str = string_add_chars(str, s, p - s);
	s = p + 1;
	if (!*p || *p == '"')
	    break;
	if (*s)
	    str = string_addc(str, *s++);
    }
    *sptr = s;
    return str;
}

string_t *string_add_unparsed(string_t *str, char *s, int len) {
    int i;

    str = string_addc(str, '"');

    /* Add characters to string, escaping quotes and backslashes. */
    while (1) {
	for (i = 0; i < len && s[i] != '"' && s[i] != '\\'; i++);
	str = string_add_chars(str, s, i);
	if (i < len) {
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

char *regerror(char *msg) {
    static char *regexp_error;

    if (msg)
	regexp_error = msg;
    return regexp_error;
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
INTERNAL string_t *prepare_to_modify(string_t *str, int start, int len) {
    string_t *cnew;
    int need_to_move, need_to_resize, size;

    /* Figure out if we need to resize the string or move its contents.  Moving
     * contents takes precedence. */
    need_to_resize = (len - start) * 4 < str->size;
    need_to_resize = need_to_resize && str->size > STARTING_SIZE;
    need_to_resize = need_to_resize || (str->size < len);
    need_to_move = (str->refs > 1) || (need_to_resize && start > 0);

    if (need_to_move) {
        /* Move the string's contents into a new list. */
        cnew = string_new(len);
        MEMCPY(cnew->s, str->s + start, (len > str->len) ? str->len : len);
        cnew->len = len;
        string_discard(str);
        return cnew;
    } else if (need_to_resize) {
        /* Resize the list.  We can assume that list->start == start == 0. */
        str->len = len;
#if DISABLED
        size = STARTING_SIZE;
        while (size < len)
            size = size * 2 + MALLOC_DELTA;
#else
        size = len + 1;
#endif
        str = (string_t *)erealloc(str, sizeof(string_t)+(size * sizeof(char)));
        str->size = size;
        return str;
    } else {
        if (str->reg) {
            efree(str->reg);
            str->reg = NULL;
        }
        str->start = start;
        str->len = len;
        return str;
    }
}

