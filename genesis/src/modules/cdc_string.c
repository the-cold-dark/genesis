/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_string.c
// ---
// Function operators acting on strings.
*/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "y.tab.h"
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "match.h"
#include "util.h"
#include "memory.h"

void op_strlen(void) {
    data_t *args;
    int len;

    /* Accept a string to take the length of. */
    if (!func_init_1(&args, STRING))
	return;

    /* Replace the argument with its length. */
    len = string_length(args[0].u.str);
    pop(1);
    push_int(len);
}

void op_substr(void) {
    int num_args, start, len, string_len;
    data_t *args;

    /* Accept a string for the initial string, an integer specifying the start
     * of the substring, and an optional integer specifying the length of the
     * substring. */
    if (!func_init_2_or_3(&args, &num_args, STRING, INTEGER, INTEGER))
	return;

    string_len = string_length(args[0].u.str);
    start = args[1].u.val - 1;
    len = (num_args == 3) ? args[2].u.val : string_len - start;

    /* Make sure range is in bounds. */
    if (start < 0) {
	cthrow(range_id, "Start (%d) is less than one.", start + 1);
    } else if (len < 0) {
	cthrow(range_id, "Length (%d) is less than zero.", len);
    } else if (start + len > string_len) {
	cthrow(range_id,
	      "The substring extends to %d, past the end of the string (%d).",
	      start + len, string_len);
    } else {
	/* Replace first argument with substring, and pop other arguments. */
	anticipate_assignment();
	args[0].u.str = string_substring(args[0].u.str, start, len);
	pop(num_args - 1);
    }
}

void op_explode(void) {
    int num_args, sep_len, len, want_blanks;
    data_t *args, d;
    list_t *exploded;
    char *sep, *s, *p, *q;
    string_t *word;

    /* Accept a string to explode and an optional string for the word
     * separator. */
    if (!func_init_1_to_3(&args, &num_args, STRING, STRING, 0))
	return;

    want_blanks = (num_args == 3) ? data_true(&args[2]) : 0;
    if (num_args >= 2) {
	sep = string_chars(args[1].u.str);
	sep_len = string_length(args[1].u.str);
    } else {
	sep = " ";
	sep_len = 1;
    }

    if (!*sep) {
      cthrow(range_id, "Null string as separator.");
      return;
    }

    s = string_chars(args[0].u.str);
    len = string_length(args[0].u.str);

    exploded = list_new(0);
    p = s;
    for (q = strcstr(p, sep); q; q = strcstr(p, sep)) {
	if (want_blanks || q > p) {
	    /* Add the word. */
	    word = string_from_chars(p, q - p);
	    d.type = STRING;
	    d.u.str = word;
	    exploded = list_add(exploded, &d);
	    string_discard(word);
	}
	p = q + sep_len;
    }

    if (*p || want_blanks) {
	/* Add the last word. */
	word = string_from_chars(p, len - (p - s));
	d.type = STRING;
	d.u.str = word;
	exploded = list_add(exploded, &d);
	string_discard(word);
    }

    /* Pop the arguments and push the list onto the stack. */
    pop(num_args);
    push_list(exploded);
    list_discard(exploded);
}

void op_strsub(void) {
    int len, search_len, replace_len;
    data_t *args;
    char *search, *replace, *s, *p, *q;
    string_t *subbed;

    /* Accept a base string, a search string, and a replacement string. */
    if (!func_init_3(&args, STRING, STRING, STRING))
	return;

    s = string_chars(args[0].u.str);
    len = string_length(args[0].u.str);
    search = string_chars(args[1].u.str);
    search_len = string_length(args[1].u.str);
    replace = string_chars(args[2].u.str);
    replace_len = string_length(args[2].u.str);

    if (*s == NULL || *search == NULL) {
        subbed = string_dup(args[0].u.str);
    } else {
        subbed = string_new(search_len);
        p = s;
        for (q = strcstr(p, search); q; q = strcstr(p, search)) {
            subbed = string_add_chars(subbed, p, q - p);
            subbed = string_add_chars(subbed, replace, replace_len);
            p = q + search_len;
        }
    
        subbed = string_add_chars(subbed, p, len - (p - s));
    }

    /* Pop the arguments and push the new string onto the stack. */
    pop(3);
    push_string(subbed);
    string_discard(subbed);
}

/* Pad a string on the left (positive length) or on the right (negative
 * length).  The optional third argument gives the fill character. */
void op_pad(void) {
    int num_args, len, padding, filler_len;
    data_t *args;
    char *filler;
    string_t *padded;

    if (!func_init_2_or_3(&args, &num_args, STRING, INTEGER, STRING))
	return;

    if (num_args == 3) {
	filler = string_chars(args[2].u.str);
	filler_len = string_length(args[2].u.str);
    } else {
	filler = " ";
	filler_len = 1;
    }

    len = (args[1].u.val > 0) ? args[1].u.val : -args[1].u.val;
    padding = len - string_length(args[0].u.str);

    /* Construct the padded string. */
    anticipate_assignment();
    padded = args[0].u.str;
    if (padding == 0) {
	/* Do nothing.  Easiest case. */
    } else if (padding < 0) {
	/* We're shortening the string.  Almost as easy. */
	padded = string_truncate(padded, len);
    } else if (args[1].u.val > 0) {
	/* We're lengthening the string on the right. */
	padded = string_add_padding(padded, filler, filler_len, padding);
    } else {
	/* We're lengthening the string on the left. */
	padded = string_new(padding + args[0].u.str->len);
	padded = string_add_padding(padded, filler, filler_len, padding);
	padded = string_add(padded, args[0].u.str);
	string_discard(args[0].u.str);
    }
    args[0].u.str = padded;

    /* Discard all but the first argument. */
    pop(num_args - 1);
}

void op_match_begin(void)
{
    data_t *args;
    int sep_len, search_len;
    char *sep, *search, *s, *p;
    int num_args;

    /* Accept a base string, a search string, and an optional separator. */
    if (!func_init_2_or_3(&args, &num_args, STRING, STRING, STRING))
	return;

    s = string_chars(args[0].u.str);

    search = string_chars(args[1].u.str);
    search_len = string_length(args[1].u.str);

    if (num_args > 2) {
      sep = string_chars(args[2].u.str);
      sep_len = string_length(args[2].u.str);
    } else {
      sep = " ";
      sep_len = 1;
    }

    for (p = s - sep_len; p; p = strcstr(p + 1, sep)) {
	/* We found a separator; see if it's followed by search. */
	if (strnccmp(p + sep_len, search, search_len) == 0) {
	    pop(num_args);
	    push_int(1);
	    return;
	}
    }

    pop(num_args);
    push_int(0);
}

/* Match against a command template. */
void op_match_template(void) {
    data_t *args;
    list_t *fields;
    char *ctemplate, *str;

    /* Accept a string for the template and a string to match against. */
    if (!func_init_2(&args, STRING, STRING))
	return;

    ctemplate = string_chars(args[0].u.str);
    str = string_chars(args[1].u.str);

    fields = match_template(ctemplate, str);

    pop(2);
    if (fields) {
	push_list(fields);
	list_discard(fields);
    } else {
	push_int(0);
    }
}

/* Match against a command template. */
void op_match_pattern(void) {
    data_t *args;
    list_t *fields;
    char *pattern, *str;

    /* Accept a string for the pattern and a string to match against. */
    if (!func_init_2(&args, STRING, STRING))
	return;

    pattern = string_chars(args[0].u.str);
    str = string_chars(args[1].u.str);

    fields = match_pattern(pattern, str);

    pop(2);
    if (!fields) {
	push_int(0);
	return;
    }

    /* fields is backwards.  Reverse it. */
    fields = list_reverse(fields);

    push_list(fields);
    list_discard(fields);
}

void op_match_regexp(void) {
    data_t * args,
             d;
    regexp * reg;
    list_t * fields = (list_t *)0;
    list_t * elemlist;
    int      num_args,
             case_flag,
             i;
    char   * s;

    if (!func_init_2_or_3(&args, &num_args, STRING, STRING, 0))
	return;

    case_flag = (num_args == 3) ? data_true(&args[2]) : 0;

    /* note: this regexp is free'd by string_discard() */
    reg = string_regexp(args[0].u.str);
    if (!reg) {
	cthrow(regexp_id, "%s", regerror(NULL));
	return;
    }

    /* Execute the regexp. */
    s = string_chars(args[1].u.str);
    if (regexec(reg, s, case_flag)) {
	/* Build the list of fields. */
	fields = list_new(NSUBEXP);
	for (i = 0; i < NSUBEXP; i++) {
	    elemlist = list_new(2);

	    d.type = INTEGER;
            /* BUG: backwards logic broke regexp matching
               found by Miroslav Silovic (Jenner) */
	    if (!reg->startp[i]) {
		d.u.val = 0;
		elemlist = list_add(elemlist, &d);
		elemlist = list_add(elemlist, &d);
	    } else {
		d.u.val = reg->startp[i] - s + 1;
		elemlist = list_add(elemlist, &d);
		d.u.val = reg->endp[i] - reg->startp[i];
		elemlist = list_add(elemlist, &d);
	    }

	    d.type = LIST;
	    d.u.list = elemlist;
	    fields = list_add(fields, &d);
	}
    }

    pop(num_args);
    if (fields) {
	push_list(fields);
	list_discard(fields);
    } else {
	push_int(0);
    }
}

/* Encrypt a string. */
void op_crypt(void) {
    int num_args;
    data_t *args;
    char *s, *encrypted;
    string_t *str;

    /* Accept a string to encrypt and an optional salt. */
    if (!func_init_1_or_2(&args, &num_args, STRING, STRING))
	return;
    if (num_args == 2 && string_length(args[1].u.str) != 2) {
	cthrow(salt_id, "Salt (%S) is not two characters.", args[1].u.str);
	return;
    }

    s = string_chars(args[0].u.str);

    if (num_args == 2) {
	encrypted = crypt_string(s, string_chars(args[1].u.str));
    } else {
	encrypted = crypt_string(s, NULL);
    }

    pop(num_args);
    str = string_from_chars(encrypted, strlen(encrypted));
    push_string(str);
    string_discard(str);
}

void op_uppercase(void) {
    data_t * args;

    /* Accept a string to uppercase. */
    if (!func_init_1(&args, STRING))
	return;

    args[0].u.str = string_uppercase(args[0].u.str);
}

void op_lowercase(void) {
    data_t *args;

    /* Accept a string to uppercase. */
    if (!func_init_1(&args, STRING))
	return;

    args[0].u.str = string_lowercase(args[0].u.str);
}

void op_strcmp(void) {
    data_t *args;
    int val;

    /* Accept two strings to compare. */
    if (!func_init_2(&args, STRING, STRING))
	return;

    /* Compare the strings case-sensitively. */
    val = strcmp(string_chars(args[0].u.str), string_chars(args[1].u.str));
    pop(2);
    push_int(val);
}

/*
// -------------------------------------------------------------
//  %<args><type>
// 
// types: d or D       (literal data),
//        l or L       (string -- left aligned)
//        r or R       (string -- right)
//        c or C       (string -- centered)
//        e or E       (string, breaks with an elipse after pad width)
// 
// args are integers, plus an optional colon seperator, which specifies
// the fill character.  If the fill character is any of the special
// characters, prefix it with a slash.  A period may eventually specify
// precision, with 'f'.  Capitalized versions of each string will
// cut the string, when it reaches the end of the specified padding.
// 
//    "%r", "test"      => "test"
//    "%l", "test"      => "test"
//    "%c", "test"      => "test"
//    "%d", "test"      => "\"test\""
// 
//    "%10r", "test"    => "      test"
//    "%10l", "test"    => "test      "
//    "%10c", "test"    => "   test   "
//    "%10:|r", "test"  => "||||||test"
//    "%10:|l", "test"  => "test||||||"
//    "%10:|c", "test"  => "|||test|||"
// 
//    "%5e", "testing"  => "te..."
// -------------------------------------------------------------
*/

void op_strfmt(void) {
    data_t   * argv,
             * arg;
    string_t * str,
             * value;
    list_t   * args;
    char     * fmt,
             * s,
             * tmp,
               pchar;
    int        len, pad;

    /* accept two arguments, second is a list for the format */
    if (!func_init_2(&argv, STRING, LIST))
	return;

    fmt = string_chars(argv[0].u.str);
    len = argv[0].u.str->len;
    args = argv[1].u.list;
    str = string_new(0);
    arg = list_first(argv[1].u.list);

    for (;;) {
        s = strchr(fmt, '%');
        if (s == NULL || s[1] == NULL) {
            str = string_add_chars(str, fmt, strlen(fmt));
            break;
        }

        str = string_add_chars(str, fmt, s - fmt);
        s++;
        len -= (s - fmt);

        if (*s == '%') {
            str = string_addc(str, '%');
            continue;
        }

        pad = 0;
        pchar = ' ';

        /* get the pad width */
        if (isdigit(*s)) {
            pad = (int) atol(s);
            while (len && isdigit(*s)) {
                s++;
                len--;
            }

            /* arbitrary restriction on pad length,
               dont want them making it too big and chomping memory */
            if (pad > 64)
                pad = 64;
        }

        /* get the pad char */
        if (*s == ':' && len) {
            s++;
            pchar = *s++;
            len -= 2;
        }

        /* invalid format, just abort, they need to know when it is wrong */
        if (len <= 0) {
            string_discard(str);
            cthrow(type_id, "Invalid format, ends in control sequence.");
            return;
        }

        if (!arg) {
            s++;
            continue;
        }

        if (*s != 'd' && arg->type == STRING)
            value = string_dup(arg->u.str);
        else if (*s != 'd' && arg->type == SYMBOL) {
            tmp = ident_name(arg->u.symbol);
            value = string_from_chars(tmp, strlen(tmp));
        } else
            value = data_to_literal(arg);

        switch (*s) {
            case 'L':
            case 'S':
                if (pad == 0)
                    break;
                if (value->len > pad) {
                    value = string_truncate(value, pad);
                } else {
                    pad -= value->len;
                    while (pad-- > 0)
                        value = string_addc(value, pchar);
                }
                str = string_add(str, value);
                break;
            case 'd':
            case 's':
            case 'l':
                pad -= value->len;
                while (pad-- > 0)
                    value = string_addc(value, pchar);
                str = string_add(str, value);
                break;
            case 'R':
                if (pad == 0)
                    break;
                if (value->len > pad) {
                    value = string_truncate(value, pad);
                } else {
                    pad -= value->len;
                    while (pad-- > 0)
                        str = string_addc(str, pchar);
                }
                str = string_add(str, value);
                break;
            case 'r':
                pad -= value->len;
                while (pad-- > 0)
                    str = string_addc(str, pchar);
                str = string_add(str, value);
                break;
            case 'C':
                if (pad == 0)
                    break;
                if (value->len > pad) {
                    value = string_truncate(value, pad);
                } else {
                    pad -= value->len;
                    pad = pad / 2;
                    while (pad-- > 0) {
                        str = string_addc(str, pchar);
                        value = string_addc(value, pchar);
                    }
                }
                str = string_add(str, value);
                break;
            case 'c':
                if (pad >= value->len) {
                    pad -= value->len;
                    pad = pad / 2;
                    while (pad-- > 0) {
                        str = string_addc(str, pchar);
                        value = string_addc(value, pchar);
                    }
                }
                str = string_add(str, value);
                break;
            case 'E':
            case 'e':
                if (pad == 0)
                    break;
                if (pad <= 3) {
                    string_discard(str);
                    string_discard(value);
                    cthrow(type_id,
                           "Elipse pad length must be at least 4 or more.");
                    return;
                }
                if (value->len > pad) {
                    if (value->len - (pad - 3) > 0)
                        pad = pad - 3;
                    value = string_truncate(value, pad);
                    value = string_add_chars(value, "...", 3);
                } else {
                    pad -= value->len;
                    while (pad-- > 0)
                        value = string_addc(value, pchar);
                }
                str = string_add(str, value);
                break;
            default:
                /* invalid format, abort */
                string_discard(str);
                string_discard(value);
                cthrow(type_id, "Invalid format, unknown control sequence.");
                return;
        }
        string_discard(value);
        arg = list_next(argv[1].u.list, arg);
        fmt = ++s;
        len--;
    }

    pop(2);
    push_string(str);
    string_discard(str);
}

