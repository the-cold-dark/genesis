/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: match.c
// ---
// generic string frobbing and matching routines
*/

#include "config.h"
#include "defs.h"

#include <string.h>
#include <ctype.h>
#include "match.h"
#include "memory.h"
#include "cdc_types.h"
#include "data.h"
#include "cdc_string.h"
#include "util.h"
#include "execute.h"

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

static Field * fields;
static int field_pos, field_size;
    
void init_match(void) {
    fields = EMALLOC(Field, FIELD_STARTING_SIZE);
    field_size = FIELD_STARTING_SIZE;
}

list_t * match_template(char *ctemplate, char *s) {
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
INTERNAL char *match_coupled_wildcard(char *ctemplate, char *s) {
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
INTERNAL char * match_wildcard(char *ctemplate, char *s) {
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
INTERNAL char * match_word_pattern(char *ctemplate, char *s) {
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
INTERNAL void add_field(char *start, char *end, int strip) {
    if (field_pos >= field_size) {
	field_size = field_size * 2 + MALLOC_DELTA;
	fields = EREALLOC(fields, Field, field_size);
    }
    fields[field_pos].start = start;
    fields[field_pos].end = end;
    fields[field_pos].strip = strip;
    field_pos++;
}

/* Returns a backwards list of fields if <s> matches the
   pattern <pattern>, or NULL if it doesn't. */
list_t * match_pattern(char *pattern, char *s) {
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

list_t * match_regexp(string_t * reg, char * s, int sensitive) {
    list_t * fields = (list_t *) NULL,
           * elemlist; 
    regexp * rx;
    data_t   d;
    int      i;

    if ((rx = string_regexp(reg)) == (regexp *) NULL) {
        cthrow(regexp_id, "%s", regerror(NULL));
        return NULL;
    }

    if (regexec(rx, s, sensitive)) {
        /* Build the list of fields. */
        fields = list_new(NSUBEXP);
        for (i = 0; i < NSUBEXP; i++) {
            elemlist = list_new(2);
    
            d.type = INTEGER; 
            if (!rx->startp[i]) {
                d.u.val = 0;
                elemlist = list_add(elemlist, &d);
                elemlist = list_add(elemlist, &d);
            } else {
                d.u.val = rx->startp[i] - s + 1;
                elemlist = list_add(elemlist, &d);
                d.u.val = rx->endp[i] - rx->startp[i];
                elemlist = list_add(elemlist, &d);
            }   

            d.type = LIST;
            d.u.list = elemlist;
            fields = list_add(fields, &d);
            list_discard(elemlist);
        }    
    }        
             
    return fields;
}

/* similar to match_regexp, except for it returns the string(s)
   which actually matched. */

#define REGSTR(rx, pos) (string_from_chars(rx->startp[pos], \
                                    rx->endp[pos] - rx->startp[pos]))

list_t * regexp_matches(string_t * reg, char * s, int sensitive) {
    list_t * fields;
    regexp * rx;
    data_t   d;
    int      i,
             size;

    if ((rx = string_regexp(reg)) == (regexp *) NULL) {
        cthrow(regexp_id, "%s", regerror(NULL));
        return NULL;
    }

    if (!regexec(rx, s, sensitive))
        return NULL;

    /* size the results */
    for (size=1; size < NSUBEXP && rx->startp[size] != NULL; size++);

    d.type = STRING;
    
    if (size == 1) {
        fields = list_new(1);
        d.u.str = REGSTR(rx, 0); 
        fields = list_add(fields, &d);
        string_discard(d.u.str); 
    } else {
        fields = list_new(size-1);
        for (i = 1; i < size && rx->startp[i] != NULL; i++) {
            d.u.str = REGSTR(rx, i);
            fields = list_add(fields, &d);
            string_discard(d.u.str);
        }
    }

    return fields;
}

int parse_strsed_args(char * args, int * global, int * sensitive) {
    while (*args != (char) NULL) {
        switch (*args) {
            case 'g': /* global */
                *global=1;
                break;
            case 's': /* single */
                *global=0;
                break;
            case 'c': /* case sensitive */
                *sensitive=1;
                break;
            case 'i': /* case insensitive */
                *sensitive=0;
                break;
            default:
                return 0;
        }
        args++; 
    }
    return 1;
}

#define x_THROW(_cthrow_) {\
        *err=1;\
        cthrow _cthrow_;\
        return NULL;\
    }

string_t * strsed(string_t * reg,  /* the regexp string */
                  string_t * ss,   /* the string to match against */
                  string_t * rs,   /* the replacement string */
                  int global,      /* globally match? */
                  int sensitive,   /* case sensitive? */
                  int mult,        /* size multiplier */
                  int * err)       /* did we have a boo boo? */
{
    register regexp * rx;
    string_t * out;
    char     * s = string_chars(ss),/* start */
             * p,                   /* pointer */
             * q,                   /* couldn't think of anything better */
             * r;                   /* replace */
    register int i, x;
    int      size=1,
             slen = string_length(ss),
             rlen = string_length(rs);

    err = 0;

    /* Compile the regexp, note: it is free'd by string_discard() */
    if ((rx = string_regexp(reg)) == (regexp *) NULL)
        x_THROW((regexp_id, "%s", regerror(NULL)))

    /* initial regexp execution */
    if (!regexec(rx, s, sensitive))
        return NULL;

    for (; size < NSUBEXP && rx->startp[size] != (char) NULL; size++);

    if (size == 1) { /* a constant, this is the easy one */
        if (global) {
            /* die after 100 subs, magic numbers yay */
            int depth = 100;
            p = s;
            out = string_new(slen + (rlen * mult));

            /* the sub loop; see, do/while loops can be useful */
            do {
                if (!--depth) {
                    string_discard(out);
                    x_THROW((maxdepth_id, "Max substitution depth exceeded"))
                }
                if ((i = rx->startp[0] - p))
                    out = string_add_chars(out, p, i);
                if (rlen)
                    out = string_add(out, rs);
                p = rx->endp[0];
            } while (p && regexec(rx, p, sensitive));

            /* add the end on */
            if ((i = (s + slen) - p))
                out = string_add_chars(out, p, i);

        } else {
            /* new string, exact size */
            out = string_new(slen + rlen - (rx->endp[0] - rx->startp[0]));

            if ((i = rx->startp[0] - s))
                out = string_add_chars(out, s, i);
            if (rlen)
                out = string_add(out, rs);
            if ((i = (s + slen) - rx->endp[0]))
                out = string_add_chars(out, rx->endp[0], i);
        }
    } else { /* rrg, now we have fun */
        if (global) {  /* they would, the bastards */
            int depth = 100;
            char * rxs = s;
    
            out = string_new(slen + ((rlen * size) * mult));
    
            if ((i = rx->startp[0] - s))
                out = string_add_chars(out, s, i);

            r = string_chars(rs);

            rxs = rx->startp[0];
            do {
                if (!--depth) {
                    string_discard(out);
                    x_THROW((maxdepth_id, "Max substitution depth exceeded"))
                }

                if ((i = rx->startp[0] - rxs))
                    out = string_add_chars(out, rxs, i);

                for (p = r, q = strchr(p, '%'); q; q = strchr(p, '%')) {
                    out = string_add_chars(out, p, q - p);

                    q++;

                    x = *q - (int) '0';

                    if (!x || x > 9) {
                        string_discard(out);
                        x_THROW((perm_id, "Subs can only be 1-9"))
                    }

                    if (rx->startp[x] != NULL && (i=rx->endp[x]-rx->startp[x]))
                        out = string_add_chars(out, rx->startp[x], i);

                    q++;
                    p = q;
                }

                if ((i = (r + rlen) - p))
                    out = string_add_chars(out, p, i);

                rxs = rx->endp[0];
            } while (rxs && regexec(rx, rxs, sensitive));

            if ((i = (s + slen) - rxs))
                out = string_add_chars(out, rxs, i);

        } else {
            out = string_new(rlen + slen);

            if ((i = rx->startp[0] - s))
                out = string_add_chars(out, s, i);

            p = r = string_chars(rs);

            for (q = strchr(p, '%'); q; q = strchr(p, '%')) {
                out = string_add_chars(out, p, q - p);

                q++;

                x = *q - (int) '0';

                if (!x || x > 9) {
                    string_discard(out);
                    x_THROW((perm_id, "Subs can only be 1-9"))
                }

                if (rx->startp[x] != NULL && (i=rx->endp[x]-rx->startp[x]))
                    out = string_add_chars(out, rx->startp[x], i);

                q++; 
                p = q;
            }

            if ((i = (r + rlen) - p))
                out = string_add_chars(out, p, i);

            if ((i = (s + slen) - rx->endp[0]))
                out = string_add_chars(out, rx->endp[0], i);
        }
    }

    return out;
}

#undef x_THROW

/*
// -------------------------------------------------------------
// %<fmt_type>
//
// %<pad>.<precision>{fill}<fmt_type>
//
// Format Types:
//
//     d,D       literal data
//     s,S,l,L   any data, align left
//     r,R       any data, align right
//     c,C       any data, align centered
//     e         any data, align left with an elipse
//
// Caplitalized types will crop the string if/when it reaches the 'pad'
// length, otherwise the string will overflow past the pad length.
//
// Examples:
//
//    "%r", "test"        => "test"
//    "%l", "test"        => "test"
//    "%c", "test"        => "test"
//    "%d", "test"        => "\"test\""
//    "%10r", "test"      => "      test"
//    "%10l", "test"      => "test      "
//    "%10c", "test"      => "   test   "
//    "%10{|>}r", "test"  => "|>|>|>test"
//    "%10{|>}l", "test"  => "test|>|>|>"
//    "%10{|>}c", "test"  => "|>|test|>|"
//    "%.2l", 1.1214      => "1.12"
//    "%10.3{0}r", 1.1214 => "000001.121"
//    "%10.3{0}l", 1.1214 => "1.12100000"
//    "%5e", "testing"    => "te..."
//
// -------------------------------------------------------------
*/

#define NEXT_VALUE() \
    switch (args[cur].type) {\
        case STRING:\
            value = string_dup(args[cur].u.str);\
            break;\
        case SYMBOL:\
            tmp = ident_name(args[cur].u.symbol);\
            value = string_from_chars(tmp, strlen(tmp));\
            break;\
        case FLOAT:\
            sprintf(buf, "%.*f", prec, (double) args[cur].u.fval); \
            value = string_from_chars(buf, strlen(buf));\
            break;\
        default:\
            value = data_to_literal(&args[cur]);\
            break;\
    }

#define x_THROW(_what_) { \
    cthrow _what_; \
    return NULL; \
}

string_t * strfmt(string_t * str, data_t * args, int argc) {
    string_t * out,
             * value;
    register char * s;
    char     * fmt,
             * tmp,
               buf[LINE],
               fill[LINE];
    register int pad, prec, trunc;
    int        cur = -1;

    fmt = string_chars(str);

    /* better more than less and having to resize */
    out = string_new(string_length(str) * 2);

    for (;;) {
        s = strchr(fmt, '%');

        if (s == (char) NULL || *s == (char) NULL) {
            out = string_add_chars(out, fmt, strlen(fmt));
            break;
        }

        out = string_add_chars(out, fmt, s - fmt);
        s++;

        if (*s == '%') {
            out = string_addc(out, '%');
            fmt = ++s;
            continue;
        }

        if (++cur >= argc) {
            string_discard(out);
            x_THROW((type_id, "Not enough arguments for format."))
        }

        pad = prec = trunc = 0;
        if (*s == '*') {
            if (args[cur].type != INTEGER)
                x_THROW((type_id, "Argument for '*' is not an integer."))
            pad = args[cur].u.val;
            s++;
            if (++cur >= argc) {
                string_discard(out);
                x_THROW((type_id, "Not enough arguments for format."))
            }
        } else {
            while (isdigit(*s))
                pad = pad * 10 + *s++ - '0';
        }

        if (*s == '.') {
            s++;
            if (*s == '*') {
                if (args[cur].type != INTEGER)
                    x_THROW((type_id, "Argument for '*' is not an integer."))
                prec = args[cur].u.val;
                s++;
                if (++cur >= argc) {
                    string_discard(out);
                    x_THROW((type_id, "Not enough arguments for format."))
                }
            } else {
                while (isdigit(*s))
                    prec = prec * 10 + *s++ - '0';
            }
        }

        /* get the pad char */
        if (*s == '{') {
            int    x = 0;

            s++;
            for (; *s && *s != '}'; s++) {
                if (s[0] == '\\' && (s[1] == '\\' || s[1] == '}'))
                    s++;
                fill[x++] = *s;
            }
            fill[x] = (char) NULL;
            s++;
        } else {
            fill[0] = ' ';
            fill[1] = (char) NULL;
        }

        /* invalid format, just abort, they need to know when it is wrong */
        if (*s == (char) NULL) {
            string_discard(out);
            x_THROW((type_id, "Invalid format"))
        }

        switch (*s) {
            case 'D':
                trunc++;
            case 'd':
                value = data_to_literal(&args[cur]);
                goto fmt_left;
            case 'S':
            case 'L':
                trunc++;
            case 's':
            case 'l':
                NEXT_VALUE()

                fmt_left:

                if (pad) {
                    if (trunc && string_length(value) > pad)
                        value = string_truncate(value, pad);
                    else if (string_length(value) < pad)
                        value=string_add_padding(value,fill,strlen(fill),pad-string_length(value));
                }

                break;

            case 'R':
                trunc++;
            case 'r':
                NEXT_VALUE()

                if (pad) {
                    if (trunc && string_length(value) > pad)
                        value = string_truncate(value, pad);
                    else if (string_length(value) < pad) {
                        string_t * new = string_new(pad + string_length(value));
                        new = string_add_padding(new, fill, strlen(fill), pad-string_length(value));
                        new = string_add(new, value);
                        string_discard(value);
                        value = new;
                    }
                }

                break;
            case 'C':
                trunc++;
            case 'c':
                NEXT_VALUE()

                if (pad) {
                    if (trunc && string_length(value) > pad)
                        value = string_truncate(value, pad);
                    else if (string_length(value) < pad) {
                        int size = (pad - string_length(value)) / 2;
                        string_t * new = string_new(pad + string_length(value));
                        new = string_add_padding(new, fill, strlen(fill),size);
                        new = string_add(new, value);
                        new = string_add_padding(new, fill, strlen(fill),size);
                        if ((pad - string_length(value))%2)
                            new = string_addc(new, fill[0]);
                        string_discard(value);
                        value = new;
                    }
                }

                break;
            case 'e':
                NEXT_VALUE();
                if (pad) {
                    if (pad <= 3) {
                        string_discard(out);
                        x_THROW((type_id,
                           "Elipse pad length must be at least 4 or more."))
                    }
                    if (string_length(value) > pad) {
                        value = string_truncate(value, pad - 3);
                        value = string_add_chars(value, "...", 3);
                    } else if (string_length(value) < pad) {
                        value=string_add_padding(value,fill,strlen(fill),pad);
                    }
                }
                break;
            default: {
                char rrk[] = {NULL, NULL};
                rrk[0] = *s;
                x_THROW((error_id, "Unknown format type '%s'.", rrk))
            }
        }

        out = string_add(out, value);
        string_discard(value);

        fmt = ++s;
    }

    return out;
}

