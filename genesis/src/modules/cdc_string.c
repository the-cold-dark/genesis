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

#define NATIVE_MODULE "$string"

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

NATIVE_METHOD(strlen) {
    INIT_1_ARG(STRING)

    RETURN_INTEGER(string_length(_STR(ARG1)))
}

NATIVE_METHOD(substr) {
    int  start,
         len,
         string_len;

    INIT_2_OR_3_ARGS(STRING, INTEGER, INTEGER)

    string_len = string_length(_STR(ARG1));
    start = _INT(ARG2) - 1;
    len = (argc == 3) ? _INT(ARG3) : string_len - start;

    /* Make sure range is in bounds. */
    if (start < 0)
        THROW((range_id, "Start (%d) is less than one.", start + 1))
    else if (len < 0)
        THROW((range_id, "Length (%d) is less than zero.", len))
    else if (start + len > string_len)
        THROW((range_id,
              "The substring extends to %d, past the end of the string (%d).",
              start + len, string_len))

    RETURN_STRING(string_substring(_STR(ARG1), start, len))
}

NATIVE_METHOD(explode) {
    int        sep_len=1,
               len,
               want_blanks=0;
    data_t     d;
    list_t   * exploded;
    string_t * word;
    char     * sep = " ",
             * s,
             * p,
             * q;
    DEF_args;

    switch(ARG_COUNT) {
        case 3:  want_blanks = data_true(&args[2]);
        case 2:  INIT_ARG2(STRING)
                 sep = string_chars(_STR(ARG2));
                 sep_len = string_length(_STR(ARG2));
        case 1:  INIT_ARG1(STRING)
                 break;
        default: THROW_NUM_ERROR(ARG_COUNT, "one to three")
    }

    if (!*sep)
      THROW((range_id, "Null string as separator."))

    s   = string_chars(_STR(ARG1));
    len = string_length(_STR(ARG1));

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
    RETURN_LIST(exploded)
}

NATIVE_METHOD(strsub) {
    int len, search_len, replace_len;
    char *search, *replace, *s, *p, *q;
    string_t *subbed;

    INIT_3_ARGS(STRING, STRING, STRING)

    s = string_chars(_STR(ARG1));
    len = string_length(_STR(ARG1));
    search = string_chars(_STR(ARG2));
    search_len = string_length(_STR(ARG2));
    replace = string_chars(_STR(ARG3));
    replace_len = string_length(_STR(ARG3));

    if (*s == NULL || *search == NULL) {
        subbed = string_dup(_STR(ARG1));
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

    RETURN_STRING(subbed)
}

/* Pad a string on the left (positive length) or on the right (negative
 * length).  The optional third argument gives the fill character. */
NATIVE_METHOD(pad) {
    int len, padding, filler_len = 1;
    char     * filler = " ";
    string_t * padded;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:    INIT_ARG3(STRING)
                   filler = string_chars(_STR(ARG3));
                   filler_len = string_length(_STR(ARG3));
        case 2:    INIT_ARG2(INTEGER)
                   INIT_ARG1(STRING)
                   break;
        default:   THROW_NUM_ERROR(ARG_COUNT, "two or three")
    } 

    len = (_INT(ARG2) > 0) ? _INT(ARG2) : -_INT(ARG2);
    padding = len - string_length(_STR(ARG1));

    /* Construct the padded string. */
    padded = _STR(ARG1);
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
        padded = string_new(padding + _STR(ARG1)->len);
        padded = string_add_padding(padded, filler, filler_len, padding);
        padded = string_add(padded, _STR(ARG1));
        string_discard(_STR(ARG1));
    }

    RETURN_STRING(padded);
}

NATIVE_METHOD(match_begin) {
    int    sep_len = 1,
           search_len;
    char * sep = " ",
         * search,
         * s,
         * p;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:    INIT_ARG3(STRING)
                   sep     = string_chars(_STR(ARG3));
                   sep_len = string_length(_STR(ARG3));
        case 2:    INIT_ARG2(STRING)
                   INIT_ARG1(STRING)
                   break;
        default:   THROW_NUM_ERROR(ARG_COUNT, "two or three")
    } 

    s = string_chars(_STR(ARG1));

    search = string_chars(_STR(ARG2));
    search_len = string_length(_STR(ARG2));

    for (p = s - sep_len; p; p = strcstr(p + 1, sep)) {
        if (strnccmp(p + sep_len, search, search_len) == 0)
            RETURN_INTEGER(1)
    }

    RETURN_INTEGER(0)
}

/* Match against a command template. */
NATIVE_METHOD(match_template) {
    list_t * fields;
    char   * ctemplate,
           * str;

    INIT_2_ARGS(STRING, STRING)

    ctemplate = string_chars(_STR(ARG1));
    str = string_chars(_STR(ARG2));

    if ((fields = match_template(ctemplate, str)))
        RETURN_LIST(fields)
    else
        RETURN_INTEGER(0)
}

/* Match against a command template. */
NATIVE_METHOD(match_pattern) {
    list_t * fields;
    char   * pattern,
           * str;

    INIT_2_ARGS(STRING, STRING)

    pattern = string_chars(_STR(ARG1));
    str = string_chars(_STR(ARG2));

    if ((fields = match_pattern(pattern, str)))
        RETURN_LIST(list_reverse(fields))
    else
        RETURN_INTEGER(0)
}

NATIVE_METHOD(match_regexp) {
    list_t * fields;
    int      sensitive=0;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:  sensitive = data_true(&args[2]);
        case 2:  INIT_ARG2(STRING)
                 INIT_ARG1(STRING)
                 break;
        default: THROW_NUM_ERROR(ARG_COUNT, "two or three")
    }

    fields = match_regexp(_STR(ARG1), string_chars(_STR(ARG2)), sensitive);

    if (fields)
        RETURN_LIST(fields)
    else
        RETURN_INTEGER(0)
}

NATIVE_METHOD(regexp) {
    list_t * fields;
    int      sensitive=0;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:  sensitive = data_true(&args[2]);
        case 2:  INIT_ARG2(STRING)
                 INIT_ARG1(STRING)
                 break;
        default: THROW_NUM_ERROR(ARG_COUNT, "two or three")
    }

    fields = regexp_matches(_STR(ARG1), string_chars(_STR(ARG2)), sensitive);
    
    if (fields)
        RETURN_LIST(fields)
    else
        RETURN_INTEGER(0)
}

#define S(_x_) args[_x_].u.str

NATIVE_METHOD(strsed) {
    string_t * out;
    int        sensitive=0,
               global=0,
               mult=2,
               err=0;
    DEF_args;

    switch (ARG_COUNT) {
        case 5:  CHECK_TYPE(4, INTEGER, "fifth")
                 mult = _INT(ARG5);
                 if (mult < 0)
                     mult = 2;
        case 4:  INIT_ARG4(STRING)
                 if (!parse_strsed_args(string_chars(_STR(ARG4)),
                                        &global,
                                        &sensitive))
                     THROW((type_id,
                            "Invalid flags \"%D\", must be one of \"gcis\"",
                            _STR(ARG4)))
        case 3:  INIT_ARG3(STRING);
                 INIT_ARG2(STRING);
                 INIT_ARG1(STRING);
        default: THROW_NUM_ERROR(ARG_COUNT, "three to five")
    }
                 /* regexp *//* string *//* replace */
    out = strsed(S(ARG1), S(ARG2), S(ARG3), global, sensitive, mult, &err);

    if (!out) {
        if (err)
            return 0;
        RETURN_INTEGER(0)
    } else
        RETURN_STRING(out)
}

/* Encrypt a string. */
NATIVE_METHOD(crypt) {
    char     * s,
             * str;

    INIT_1_OR_2_ARGS(STRING, STRING)

    s = string_chars(_STR(ARG1));

    if (argc == 2) {
        if (string_length(_STR(ARG2)) != 2)
            THROW((salt_id, "Salt (%S) is not two characters.", args[1].u.str))
        str = crypt_string(s, string_chars(args[1].u.str));
    } else {
        str = crypt_string(s, NULL);
    }

    RETURN_STRING(string_from_chars(str, strlen(str)))
}

NATIVE_METHOD(uppercase) {
    INIT_1_ARG(STRING)

    RETURN_STRING(string_uppercase(_STR(ARG1)));
}

NATIVE_METHOD(capitalize) {
    char     * s;
    string_t * str;
    INIT_1_ARG(STRING)

    str = _STR(ARG1);

    if (str->refs > 1) {
        string_t * new = string_new(str->len);
        MEMCPY(new->s, string_chars(str), string_length(str));
        new->len = string_length(str);
        str = new;
    }

    s = string_chars(str);
    s[0] = UCASE(s[0]);

    RETURN_STRING(str)
}

NATIVE_METHOD(lowercase) {
    INIT_1_ARG(STRING)

    RETURN_STRING(string_lowercase(_STR(ARG1)));
}

NATIVE_METHOD(strcmp) {
    INIT_2_ARGS(STRING, STRING)

    RETURN_INTEGER(strcmp(string_chars(_STR(ARG1)), string_chars(_STR(ARG2))))
}

NATIVE_METHOD(strfmt) {
    string_t * fmt,
             * out;
    DEF_argc;
    DEF_args;

    if (!argc)
        THROW((numargs_id, "Called with no arguments, requires at least one."))

    if (stack[arg_start].type != STRING)
        THROW((type_id, "First argument (%D) not a string.", &stack[arg_start]))

    fmt = stack[arg_start].u.str;

    if (argc == 1)
        RETURN_STRING(string_dup(fmt));

    args = &stack[arg_start + 1];

    if ((out = strfmt(fmt, args, argc - 1)) == (string_t *) NULL)
        return 0;

    RETURN_STRING(out);
}

