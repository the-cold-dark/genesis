/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: ops/string.c
// ---
// string functions.
*/

#include "config.h"
#include "defs.h"

#include <string.h>
#include <ctype.h>
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "match.h"
#include "util.h"
#include "memory.h"

void func_strlen(void) {
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

void func_substr(void) {
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
    if (start < 0)
	THROW((range_id, "Start (%d) is less than one.", start + 1))
    else if (len < 0)
	THROW((range_id, "Length (%d) is less than zero.", len))
    else if (start + len > string_len)
	THROW((range_id,
	      "The substring extends to %d, past the end of the string (%d).",
	      start + len, string_len))

    /* Replace first argument with substring, and pop other arguments. */
    anticipate_assignment();
    args[0].u.str = string_substring(args[0].u.str, start, len);
    pop(num_args - 1);
}

void func_explode(void) {
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

void func_strsub(void) {
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
void func_pad(void) {
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

void func_match_begin(void) {
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
void func_match_template(void) {
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
void func_match_pattern(void) {
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

void func_match_regexp(void) {
    data_t * args;
    list_t * fields;
    int      argc,
             sensitive;

    if (!func_init_2_or_3(&args, &argc, STRING, STRING, 0))
	return;

    sensitive = (argc == 3) ? data_true(&args[2]) : 0;

    fields = match_regexp(_STR(ARG1), string_chars(_STR(ARG2)), sensitive);

    pop(argc);

    if (fields) {
        push_list(fields);
        list_discard(fields);
    } else {
        push_int(0);
    }
}

void func_regexp(void) {
    data_t * args;
    list_t * fields;
    int      argc,
             sensitive;

    if (!func_init_2_or_3(&args, &argc, STRING, STRING, 0))
        return;

    sensitive = (argc == 3) ? data_true(&args[2]) : 0;

    fields = regexp_matches(_STR(ARG1), string_chars(_STR(ARG2)), sensitive);

    pop(argc);
    if (fields) {
        push_list(fields);
        list_discard(fields);
    } else {
        push_int(0);
    }
}

void func_strsed(void) {
    data_t   * args;
    string_t * out;
    int        argc,
               sensitive=0,
               global=0,
               mult=2,
               err=0,
               arg_start = arg_starts[--arg_pos];

    args = &stack[arg_start];
    argc = stack_pos - arg_start;

    switch (argc) {
        case 5: if (args[4].type != INTEGER)
                    THROW_TYPE_ERROR(STRING, "fifth", 4)
                mult = args[4].u.val;
                if (mult < 0)
                    mult = 2;
                if (mult > 10)
                    THROW((perm_id, "You can only specify a size multiplier of 1-10, sorry!"))
        case 4: if (args[3].type != STRING)
                    THROW_TYPE_ERROR(STRING, "fourth", 3)
                if (!parse_strsed_args(string_chars(_STR(ARG4)),
                                       &global,
                                       &sensitive))
                    THROW((type_id,
                           "Invalid flags \"%D\", must be one of \"gcis\"",
                           _STR(ARG4)))
        case 3: if (args[0].type != STRING)
                    THROW_TYPE_ERROR(STRING, "first", 0)
                else if (args[1].type != STRING)
                    THROW_TYPE_ERROR(STRING, "second", 1)
                else if (args[2].type != STRING)
                    THROW_TYPE_ERROR(STRING, "third", 2)
                break;
        default:
                func_num_error(argc, "three to five");
                return;
    }
                 /* regexp *//* string *//* replace */
    out = strsed(_STR(ARG1), _STR(ARG2), _STR(ARG3), global, sensitive, mult, &err);

    if (!out) {
        if (err)
            return;
        pop(argc);
        push_int(0);
    } else {
        pop(argc);
        push_string(out);
        string_discard(out);
    }
}

/* Encrypt a string. */
void func_crypt(void) {
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

void func_uppercase(void) {
    data_t * args;

    /* Accept a string to uppercase. */
    if (!func_init_1(&args, STRING))
	return;

    args[0].u.str = string_uppercase(args[0].u.str);
}

void func_lowercase(void) {
    data_t *args;

    /* Accept a string to uppercase. */
    if (!func_init_1(&args, STRING))
	return;

    args[0].u.str = string_lowercase(args[0].u.str);
}

void func_strcmp(void) {
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

void func_strgraft(void) {
    data_t * args;
    string_t * new, * s1, * s2;
    int pos;

    if (!func_init_3(&args, STRING, INTEGER, STRING))
        return;

    pos = args[1].u.val - 1;
    s1  = args[0].u.str;
    s2  = args[2].u.str;

    if (pos > string_length(s1) || pos < 0) {
        cthrow(range_id,
               "Position %D is outside of the range of the string.",
               &args[1]);
        return;
    } else if (pos == 0) {
        s2 = string_add(s2, s1);
        new = string_dup(s2);
    } else if (pos == string_length(s1)) {
        s1 = string_add(s1, s2);
        new = string_dup(s1);
    } else {
        new = string_new(string_length(s1) + string_length(s2));
        new = string_add_chars(new, string_chars(s1), pos);
        new = string_add(new, s2);
        new =string_add_chars(new,string_chars(s1)+pos,string_length(s1)-pos+1);
    }

    pop(3);
    push_string(new);
    string_discard(new);
}

COLDC_FUNC(strfmt) {
    int        arg_start,
               argc;
    data_t   * args;
    string_t * fmt,
             * out;

    /* init ourselves for a variable number of arguments of any type */
    arg_start = arg_starts[--arg_pos];
    argc = stack_pos - arg_start;

    if (!argc)
        THROW((numargs_id, "Called with no arguments, requires at least one."))

    if (stack[arg_start].type != STRING)
        THROW((type_id, "First argument (%D) not a string.", &stack[arg_start]))

    /* leave the format on the stack, it is all they sent */
    if (argc == 1)
        return;

    fmt = stack[arg_start].u.str;
    args = &stack[arg_start + 1];

    if ((out = strfmt(fmt, args, argc - 1)) == (string_t *) NULL)
        return;

    pop(argc);
    push_string(out);
    string_discard(out);
}

