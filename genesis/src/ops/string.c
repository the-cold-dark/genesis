/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <string.h>
#include <ctype.h>
#include "operators.h"
#include "execute.h"
#include "strutil.h"
#include "util.h"
#include "crypt.h"

COLDC_FUNC(strlen) {
    cData *args;
    Int len;

    /* Accept a string to take the length of. */
    if (!func_init_1(&args, STRING))
	return;

    /* Replace the argument with its length. */
    len = string_length(args[0].u.str);
    pop(1);
    push_int(len);
}

COLDC_FUNC(substr) {
    Int num_args, start, len, string_len;
    cData *args;

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

COLDC_FUNC(explode) {
    Int      argc, sep_len;
    Bool     want_blanks;
    cData * args;
    cList * exploded;
    char   * sep;

    /* Accept a string to explode and an optional string for the word
     * separator. */
    if (!func_init_1_to_3(&args, &argc, STRING, STRING, 0))
	return;

    want_blanks = (Bool) ((argc == 3) ? data_true(&args[2]) : NO);

    if (argc >= 2) {
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

    exploded = strexplode(args[0].u.str, sep, sep_len, want_blanks);

    /* Pop the arguments and push the list onto the stack. */
    pop(argc);
    push_list(exploded);
    list_discard(exploded);
}

COLDC_FUNC(strsub) {
#if 0
    Int len, search_len, replace_len;
    cData *args;
    char *search, *replace, *s, *p, *q;
    cStr *subbed;
#endif
    cData * args;
    int     argc;
    Int     flags = RF_GLOBAL;
    cStr  * out;

    /* Accept a base string, a search string, and a replacement string. */
    if (!func_init_3_or_4(&args, &argc, STRING, STRING, STRING, STRING))
	return;

#if DISABLED
    s = string_chars(args[0].u.str);
    len = string_length(args[0].u.str);
    search = string_chars(args[1].u.str);
    search_len = string_length(args[1].u.str);
    replace = string_chars(args[2].u.str);
    replace_len = string_length(args[2].u.str);

    if (*s == (char) NULL || *search == (char) NULL) {
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
#endif
    if (argc == 4)
        flags = parse_regfunc_args(string_chars(STR4), flags);

    out = strsub(STR1, STR2, STR3, flags);

    /* Pop the arguments and push the new string onto the stack. */
    pop(argc);
    push_string(out);
    string_discard(out);
}

/* Pad a string on the left (positive length) or on the right (negative
 * length).  The optional third argument gives the fill character. */
COLDC_FUNC(pad) {
    Int num_args, len, padding, filler_len;
    cData *args;
    char *filler;
    cStr *padded;

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

COLDC_FUNC(match_begin) {
    cData *args;
    Int sep_len, search_len;
    char *sep, *search, *s, *p;
    Int num_args;

    /* Accept a base string, a search string, and an optional separator. */
    if (!func_init_2_or_3(&args, &num_args, STRING, STRING, STRING))
	return;

    s = string_chars(STR1);

    search = string_chars(STR2);
    search_len = string_length(STR2);

    if (num_args > 2) {
      sep = string_chars(args[2].u.str);
      sep_len = string_length(args[2].u.str);
      if (!sep_len)
          THROW((range_id, "Zero length separator."))
    } else {
      sep = " ";
      sep_len = 1;
    }

    /* check the beginning */
    p = strcstr(s, sep);
    if (p != s) {
        if (strnccmp(s, search, search_len) == 0) {
            pop(num_args);
            push_int(1);
            return;
        }
    }

    for (; p; p = strcstr(p + sep_len, sep)) {
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
COLDC_FUNC(match_template) {
    cData *args;
    cList *fields;
    char *ctemplate, *str;

    /* Accept a string for the template and a string to match against. */
    if (!func_init_2(&args, STRING, STRING))
	return;

    str = string_chars(STR1);
    ctemplate = string_chars(STR2);

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
COLDC_FUNC(match_pattern) {
    cData *args;
    cList *fields;
    char *pattern, *str;

    /* Accept a string for the pattern and a string to match against. */
    if (!func_init_2(&args, STRING, STRING))
	return;

    str = string_chars(STR1);
    pattern = string_chars(STR2);

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

COLDC_FUNC(match_regexp) {
    cData * args;
    cList * fields;
    Int     argc,
            sensitive;
    Bool    error;

    if (!func_init_2_or_3(&args, &argc, STRING, STRING, 0))
	return;

    sensitive = (argc == 3) ? data_true(&args[2]) : 0;

    fields = match_regexp(STR2, string_chars(STR1), sensitive, &error);

    if (fields) {
        pop(argc);
        push_list(fields);
        list_discard(fields);
    } else {
        if (error == YES) /* we actually threw an error */
            return;
        pop(argc);
        push_int(0);
    }
}

COLDC_FUNC(regexp) {
    cData * args;
    cList * fields;
    Int      argc,
             sensitive;
    Bool     error;

    if (!func_init_2_or_3(&args, &argc, STRING, STRING, 0))
        return;

    sensitive = (argc == 3) ? data_true(&args[2]) : 0;

    fields = regexp_matches(STR2, string_chars(STR1), sensitive, &error);

    if (fields) {
        pop(argc);
        push_list(fields);
        list_discard(fields);
    } else {
        if (error == YES)
            return;
        pop(argc);
        push_int(0);
    }
}

COLDC_FUNC(strsed) {
    cData   * args;
    cStr * out;
    Int        argc,
               flags = RF_NONE,
               mult=2,
               arg_start = arg_starts[--arg_pos];

    args = &stack[arg_start];
    argc = stack_pos - arg_start;

    switch (argc) {
        case 5: if (args[4].type != INTEGER)
                    THROW_TYPE_ERROR(INTEGER, "fifth", 4)
                mult = args[4].u.val;
                if (mult < 0)
                    mult = 2;
                if (mult > 10)
                    THROW((perm_id, "You can only specify a size multiplier of 1-10, sorry!"))
        case 4: if (args[3].type != STRING)
                    THROW_TYPE_ERROR(STRING, "fourth", 3)
                flags = parse_regfunc_args(string_chars(STR4), flags);
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

    if (!(out = strsed(STR2, STR1, STR3, flags, mult)))
        return;

    pop(argc);
    push_string(out);
    string_discard(out);
}

/* Encrypt a string. */
COLDC_FUNC(crypt) {
    Int     argc;
    cData * args;
    cStr  * str;

    /* Accept a string to encrypt and an optional salt. */
    if (!func_init_1_or_2(&args, &argc, STRING, STRING))
	return;

    str = strcrypt(STR1, ((argc == 2) ? (STR2) : ((cStr *) NULL)));

    pop(argc);
    push_string(str);
    string_discard(str);
}

/* Match Encrypted string. */
/* first arg: already encrypted password */
/* second arg: word which may be what the encrypted password is */
COLDC_FUNC(match_crypted) {
    cData * args;
    Int     rval;

    /* Accept a string to encrypt and an optional salt. */
    if (!func_init_2(&args, STRING, STRING))
	return;

    /* match_crypted returns F_FAILURE when it throws */
    if ((rval = match_crypted(STR1, STR2)) == F_FAILURE)
        return;

    pop(2);
    push_int(rval);
}

COLDC_FUNC(uppercase) {
    cData * args;

    /* Accept a string to uppercase. */
    if (!func_init_1(&args, STRING))
	return;

    args[0].u.str = string_uppercase(args[0].u.str);
}

COLDC_FUNC(lowercase) {
    cData   * args;

    /* Accept a string to uppercase. */
    if (!func_init_1(&args, STRING))
	return;

    args[0].u.str = string_lowercase(args[0].u.str);
}

COLDC_FUNC(strcmp) {
    cData *args;
    Int val;

    /* Accept two strings to compare. */
    if (!func_init_2(&args, STRING, STRING))
	return;

    /* Compare the strings case-sensitively. */
    val = strcmp(string_chars(args[0].u.str), string_chars(args[1].u.str));
    pop(2);
    push_int(val);
}

COLDC_FUNC(strgraft) {
    cData * args;
    cStr * new, * s1, * s2;
    Int pos;

    if (!func_init_3(&args, STRING, INTEGER, STRING))
        return;

    pos = INT2 - 1;
    s1  = STR1;
    s2  = STR3;

    if (pos > string_length(s1) || pos < 0)
        THROW((range_id, "Position %D is outside of the range of the string.",
               &args[1]));

    s1 = string_dup(s1);
    s2 = string_dup(s2);
    anticipate_assignment();
    pop(3);

    if (pos == 0) {
        s2 = string_add(s2, s1);
        push_string(s2);
    } else if (pos == string_length(s1)) {
        s1 = string_add(s1, s2);
        push_string(s1);
    } else {
        new = string_new(string_length(s1) + string_length(s2));
        new = string_add_chars(new, string_chars(s1), pos);
        new = string_add(new, s2);
        new =string_add_chars(new,string_chars(s1)+pos,string_length(s1) - pos);
        push_string(new);
        string_discard(new);
    }

    string_discard(s1);
    string_discard(s2);
}

COLDC_FUNC(strfmt) {
    Int        arg_start,
               argc;
    cData   * args;
    cStr * fmt,
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

    if ((out = strfmt(fmt, args, argc - 1)) == (cStr *) NULL)
        return;

    pop(argc);
    push_string(out);
    string_discard(out);
}

COLDC_FUNC(split) {
    Int     flags = RF_NONE;
    cList * list;

    INIT_2_OR_3_ARGS(STRING, STRING, STRING);

    if (argc == 3)
        flags = parse_regfunc_args(string_chars(STR3), flags);

    if (!(list = strsplit(STR1, STR2, flags)))
        return;

    pop(argc);
    push_list(list);
    list_discard(list);
}

COLDC_FUNC(stridx) {
    int origin;
    int r;

    INIT_2_OR_3_ARGS(STRING, STRING, INTEGER);

    if (argc == 3)
        origin = INT3;
    else
        origin = 1;

    if (!string_length(STR2))
        THROW((type_id, "No search string.")) 

    if (!string_length(STR1)) {
        pop(argc);
        push_int(0);
       return;
    }

    if ((r = string_index(STR1, STR2, origin)) == F_FAILURE)
        THROW((range_id, "Origin is beyond the range of the string."))

    pop(argc);
    push_int(r);
}

