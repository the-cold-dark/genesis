/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: match.c
// ---
// Routine for matching against a template.
*/

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "match.h"
#include "memory.h"
#include "cdc_types.h"
#include "data.h"
#include "cdc_string.h"
#include "util.h"

/* We use MALLOC_DELTA to keep the memory allocated in fields thirty-two bytes
 * less than a power of two, assuming fields are twelve bytes. */
#define MALLOC_DELTA		3
#define FIELD_STARTING_SIZE	8

typedef struct {
    char *start;
    char *end;
    int strip;			/* Strip backslashes? */
} Field;

static char *match_coupled_wildcard(char *ctemplate, char *s);
static char *match_wildcard(char *ctemplate, char *s);
static char *match_word_pattern(char *ctemplate, char *s);
static void add_field(char *start, char *end, int strip);

static Field *fields;
static int field_pos, field_size;
    
void init_match(void)
{
    fields = EMALLOC(Field, FIELD_STARTING_SIZE);
    field_size = FIELD_STARTING_SIZE;
}

list_t *match_template(char *ctemplate, char *s)
{
    char *p;
    int i, coupled;
    list_t *l;
    data_t *d;
    string_t *str;

    field_pos = 0;

    /* Strip leading spaces in template. */
    while (*ctemplate == ' ')
	ctemplate++;

    while (*ctemplate) {

	/* Skip over spaces in s. */
	while (*s == ' ')
	    s++;

	/* Is the next template entry a wildcard? */
	if (*ctemplate == '*') {
	    /* Check for coupled wildcard ("*=*"). */
	    if (ctemplate[1] == '=' && ctemplate[2] == '*') {
		ctemplate += 2;
		coupled = 1;
	    } else {
		coupled = 0;
	    }

	    /* Template is invalid if wildcard is not alone. */
	    if (ctemplate[1] && ctemplate[1] != ' ')
		return NULL;

	    /* Skip over spaces to find next token. */
	    while (*++ctemplate == ' ');

	    /* Two wildcards in a row is illegal. */
	    if (*ctemplate == '*')
		return NULL;

	    /* Match the wildcard.  This also matches the next token, if there
	     * is one, and adds the appropriate fields. */
	    if (coupled)
		s = match_coupled_wildcard(ctemplate, s);
	    else
		s = match_wildcard(ctemplate, s);
	    if (!s)
		return NULL;
	} else {
	    /* Match the word pattern.  This does not add any fields, so we
	     * do it ourselves.*/
	    p = match_word_pattern(ctemplate, s);
	    if (!p)
		return NULL;
	    add_field(s, p, 0);
	    s = p;
	}

	/* Get to next token in ctemplate, if there is one. */
	while (*ctemplate && *ctemplate != ' ')
	    ctemplate++;
	while (*ctemplate == ' ')
	    ctemplate++;
    }

    /* Ignore any trailing spaces in s. */
    while (*s == ' ')
	s++;

    /* If there's anything left over in s, the match failed. */
    if (*s)
	return NULL;

    /* The match succeeded.  Construct a list of the fields. */
    l = list_new(field_pos);
    d = list_empty_spaces(l, field_pos);
    for (i = 0; i < field_pos; i++) {
	s = fields[i].start;
	if (fields[i].strip && fields[i].end > s) {
	    str = string_new(fields[i].end - s);
	    p = (char *)memchr(s, '\\', fields[i].end - 1 - s);
	    while (p) {
		str = string_add_chars(str, s, p - s);
		str = string_addc(str, p[1]);
		s = p + 2;
		p = (char *)memchr(s, '\\', fields[i].end - 1 - s);
	    }
	    str = string_add_chars(str, s, fields[i].end - s);
	} else {
	    str = string_from_chars(s, fields[i].end - s);
	}
	d->type = STRING;
	d->u.str = str;
	d++;
    }

    return l;
}

/* Match a coupled wildcard as well as the next token, if there is one.  This
 * adds the fields that it matches. */
static char *match_coupled_wildcard(char *ctemplate, char *s)
{
    char *p, *q;

    /* Check for quoted text. */
    if (*s == '"') {
	/* Find the end of the quoted text. */
	p = s + 1;
	while (*p && *p != '"') {
	    if (*p == '\\' && p[1])
		p++;
	    p++;
	}

	/* Skip whitespace after quoted text. */
	for (q = p + 1; *q && *q == ' '; q++);

	/* Move on if next character is an equals sign. */
	if (*q == '=') {
	    for (q++; *q && *q == ' '; q++);
	    add_field(s + 1, p, 1);
	    return match_wildcard(ctemplate, q);
	} else {
	    return NULL;
	}
    }

    /* Find first occurrance of an equal sign. */
    p = strchr(s, '=');
    if (!p)
	return NULL;

    /* Add field up to first nonspace character before equalsign, and move on
     * starting from the first nonspace character after it. */
    for (q = p - 1; *q == ' '; q--);
    for (p++; *p == ' '; p++);
    add_field(s, q + 1, 0);
    return match_wildcard(ctemplate, p);
}

/* Match a wildcard.  Also match the next token, if there is one.  This adds
 * the fields that it matches. */
static char *match_wildcard(char *ctemplate, char *s)
{
    char *p, *q, *r;

    /* If no token follows the wildcard, then the match succeeds. */
    if (!*ctemplate) {
	p = s + strlen(s);
	add_field(s, p, 0);
	return p;
    }

    /* There's a word pattern to match.  Check if wildcard match is quoted. */
    if (*s == '"') {
	/* Find the end of the quoted text. */
	p = s + 1;
	while (*p && *p != '"') {
	    if (*p == '\\' && p[1])
		p++;
	    p++;
	}

	/* Skip whitespace after quoted wildcard match. */
	for (q = p + 1; *q == ' '; q++);

	/* Next token must match here. */
	r = match_word_pattern(ctemplate, q);
	if (r) {
	    add_field(s + 1, p, 1);
	    add_field(q, r, 0);
	    return r;
	} else {
	    return NULL;
	}
    } else if (*s == '\\') {
	/* Skip an initial backslash.  This is so a wildcard match can start
	 * with a double quote without being a quoted match. */
	s++;
    }

    /* There is an unquoted wildcard match.  Start by looking here. */
    p = match_word_pattern(ctemplate, s);
    if (p) {
	add_field(s, s, 0);
	add_field(s, p, 0);
	return p;
    }

    /* Look for more words to match. */
    p = s;
    while (*p) {
	if (*p == ' ') {
	    /* Skip to end of word separator and try next word. */
	    q = p + 1;
	    while (*q == ' ')
		q++;
	    r = match_word_pattern(ctemplate, q);
	    if (r) {
		/* It matches; add wildcard field and word field. */
		add_field(s, p, 0);
		add_field(q, r, 0);
		return r;
	    }
	    /* No match; continue looking at q. */
	    p = q;
	} else {
	    p++;
	}
    }

    /* No words matched, so the match fails. */
    return NULL;
}

/* Match a word pattern.  Do not add any fields. */
static char *match_word_pattern(char *ctemplate, char *s)
{
    char *p = s;
    int abbrev = 0;

    while (*ctemplate && *ctemplate != ' ' && *ctemplate != '|') {

	if (*ctemplate == '"' || *ctemplate == '*') {
	    /* Invalid characters in a word pattern; match fails. */
	    return NULL;

	} else if (*ctemplate == '?') {
	    /* A question mark tells us that the matching string can be
	     * abbreviated down to this point. */
	    abbrev = 1;

	} else if (LCASE(*p) != LCASE(*ctemplate)) {
	    /* The match succeeds if we're at the end of the word in p and
	     * abbrev is set. */
	    if (abbrev && (!*p || *p == ' '))
		return p;

	    /* Otherwise the match against this word fails.  Try the next word
	     * if there is one, or fail if there isn't.  Also catch illegal
	     * characters (* and ") in the word and return NULL if we find
	     * them. */
	    while (*ctemplate && !strchr(" |*\"", *ctemplate))
		ctemplate++;
	    if (*ctemplate == '|')
		return match_word_pattern(ctemplate + 1, s);
	    else
		return NULL;

	} else {
	    p++;
	}

	ctemplate++;
    }

    /* We came to the end of the word in the template.  If we're also at the
     * end of the word in p, then the match succeeds.  Otherwise, try the next
     * word if there is one, and fail if there isn't. */
    if (!*p || *p == ' ')
	return p;
    else if (*ctemplate == '|')
	return match_word_pattern(ctemplate + 1, s);
    else
	return NULL;
}

/* Add a field.  strip should be true if this is a field for a wildcard not at
 * the end of the template. */
static void add_field(char *start, char *end, int strip)
{
    if (field_pos >= field_size) {
	field_size = field_size * 2 + MALLOC_DELTA;
	fields = EREALLOC(fields, Field, field_size);
    }
    fields[field_pos].start = start;
    fields[field_pos].end = end;
    fields[field_pos].strip = strip;
    field_pos++;
}

/* Returns a backwards list of fields if <s> matches the pattern <pattern>, or
 * NULL if it doesn't. */
list_t *match_pattern(char *pattern, char *s)
{
    char *p, *q;
    list_t *list;
    string_t *str;
    data_t d;

    /* Locate wildcard in pattern, if any.  If there isn't any, return an empty
     * list if pattern and s are equivalent, or fail if they aren't. */
    p = strchr(pattern, '*');
    if (!p)
	return (strccmp(pattern, s) == 0) ? list_new(0) : NULL;

    /* Fail if s and pattern don't match up to wildcard. */
    if (strnccmp(pattern, s, p - pattern) != 0)
	return NULL;

    /* Consider s starting after the part that matches the pattern up to the
     * wildcard. */
    s += (p - pattern);

    /* Match always succeeds if wildcard is at end of line. */
    if (!p[1]) {
	list = list_new(1);
	str = string_from_chars(s, strlen(s));
	d.type = STRING;
	d.u.str = str;
	list = list_add(list, &d);
        string_discard(str);
	return list;
    }

    /* Find first potential match of rest of pattern. */
    for (q = s; *q && *q != p[1]; q++);

    /* As long as we have a potential match... */
    while (*q) {
	/* Check to see if this is actually a match. */
	list = match_pattern(p + 1, q);
	if (list) {
	    /* It matched.  Append a field and return the list. */
	    str = string_from_chars(s, q - s);
	    d.type = STRING;
	    d.u.str = str;
	    list = list_add(list, &d);
	    string_discard(str);
	    return list;
	}

	/* Find next potential match. */
	while (*++q && *q != p[1]);
    }

    /* Return failure. */
    return NULL;
}

