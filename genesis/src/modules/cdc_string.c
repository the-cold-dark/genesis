/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define NATIVE_MODULE "$string"

#include "cdc.h"

#include <string.h>
#include <ctype.h>
#include "strutil.h"
#include "util.h"
#include "crypt.h"

NATIVE_METHOD(strlen) {
    Int len;
    INIT_1_ARG(STRING)

    len = string_length(STR1);
    CLEAN_RETURN_INTEGER(len);
}

NATIVE_METHOD(substr) {
    Int        start,
               len,
               slen;
    cStr * str;

    INIT_2_OR_3_ARGS(STRING, INTEGER, INTEGER)

    slen = string_length(STR1);
    start = INT2 - 1;
    len = (argc == 3) ? INT3 : slen - start;

    /* Make sure range is in bounds. */
    if (start < 0)
        THROW((range_id, "Start (%d) is less than one.", start + 1))
    else if (len < 0)
        THROW((range_id, "Length (%d) is less than zero.", len))
    else if (start + len > slen)
        THROW((range_id,
              "The substring extends to %d, past the end of the string (%d).",
              start + len, slen))

    str = string_dup(STR1);

    CLEAN_STACK();
    anticipate_assignment();

    RETURN_STRING(string_substring(str, start, len));
}

NATIVE_METHOD(explode) {
    Int        sep_len=1;
    Bool       want_blanks=NO;
    cList   * exploded;
    char     * sep = " ";
    DEF_args;

    switch(ARG_COUNT) {
        case 3:  want_blanks = (Bool) data_true(&args[2]);
        case 2:  INIT_ARG2(STRING)
                 sep = string_chars(STR2);
                 sep_len = string_length(STR2);
        case 1:  INIT_ARG1(STRING)
                 break;
        default: THROW_NUM_ERROR(ARG_COUNT, "one to three")
    }

    if (!*sep)
      THROW((range_id, "Null string as separator."))

    exploded = strexplode(STR1, sep, sep_len, want_blanks);

    /* Pop the arguments and push the list onto the stack. */
    CLEAN_RETURN_LIST(exploded);
}

NATIVE_METHOD(strsub) {
#if DISABLED
    Int len, search_len, replace_len;
    char *search, *replace, *s, *p, *q;
    cStr *subbed;

    INIT_3_ARGS(STRING, STRING, STRING)

    s = string_chars(STR1);
    len = string_length(STR1);
    search = string_chars(STR2);
    search_len = string_length(STR2);
    replace = string_chars(STR3);
    replace_len = string_length(STR3);

    if (*s == (char) NULL || *search == (char) NULL) {
        subbed = string_dup(STR1);
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

    CLEAN_RETURN_STRING(subbed);
#endif
    Int     flags = RF_GLOBAL;
    cStr  * out;

    INIT_3_OR_4_ARGS(STRING, STRING, STRING, STRING);

    if (argc == 4)
        flags = parse_regfunc_args(string_chars(STR4), flags);
    
    out = strsub(STR1, STR2, STR3, flags);
    
    CLEAN_RETURN_STRING(out);
}

/* Pad a string on the left (positive length) or on the right (negative
 * length).  The optional third argument gives the fill character. */
NATIVE_METHOD(pad) {
    Int len, padding, filler_len = 1;
    char     * filler = " ";
    cStr * padded,
             * sfill = NULL,
             * str;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:    INIT_ARG3(STRING)
                   sfill = string_dup(STR3);
                   filler = string_chars(sfill);
                   filler_len = string_length(sfill);
        case 2:    INIT_ARG2(INTEGER)
                   INIT_ARG1(STRING)
                   break;
        default:   THROW_NUM_ERROR(ARG_COUNT, "two or three")
    } 

    str = string_dup(STR1);
    len = (INT2 > 0) ? INT2 : -INT2;
    padding = len - string_length(str);

    CLEAN_STACK();
    anticipate_assignment();

    /* Construct the padded string. */
    if (padding == 0) {
        /* Do nothing.  Easiest case. */
    } else if (padding < 0) {
        /* We're shortening the string.  Almost as easy. */
        str = string_truncate(str, len);
    } else if (args[1].u.val > 0) {
        /* We're lengthening the string on the right. */
        str = string_add_padding(str, filler, filler_len, padding);
    } else {
        /* We're lengthening the string on the left. */
        padded = string_new(padding + str->len);
        padded = string_add_padding(padded, filler, filler_len, padding);
        padded = string_add(padded, str);
        string_discard(str);
        str = padded;
    }

    if (sfill != NULL)
        string_discard(sfill);

    RETURN_STRING(str);
}

NATIVE_METHOD(match_begin) {
    Int    sep_len = 1,
           search_len,
           yn = 0;
    char * sep = " ",
         * search,
         * s,
         * p;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:    INIT_ARG3(STRING)
                   sep     = string_chars(STR3);
                   sep_len = string_length(STR3);
                   if (!sep_len)
                       THROW((range_id, "Zero length separator."))
        case 2:    INIT_ARG2(STRING)
                   INIT_ARG1(STRING)
                   break;
        default:   THROW_NUM_ERROR(ARG_COUNT, "two or three")
    } 

    s = string_chars(STR1);

    search = string_chars(STR2);
    search_len = string_length(STR2);

    for (p = s - sep_len; p; p = strcstr(p + 1, sep)) {
        if (strnccmp(p + sep_len, search, search_len) == 0) {
            yn = 1;
            break;
        }
    }

    CLEAN_RETURN_INTEGER(yn);
}

/* Match against a command template. */
NATIVE_METHOD(match_template) {
    cList * fields;
    char   * ctemplate,
           * str;

    INIT_2_ARGS(STRING, STRING);

    str = string_chars(STR1);
    ctemplate = string_chars(STR2);

    if ((fields = match_template(ctemplate, str))) {
        CLEAN_RETURN_LIST(fields);
    } else {
        CLEAN_RETURN_INTEGER(0);
    }
}

/* Match against a command template. */
NATIVE_METHOD(match_pattern) {
    cList * fields;
    char   * pattern,
           * str;

    INIT_2_ARGS(STRING, STRING)

    str = string_chars(STR1);
    pattern = string_chars(STR2);

    if ((fields = match_pattern(pattern, str))) {
        CLEAN_RETURN_LIST(list_reverse(fields));
    } else {
        CLEAN_RETURN_INTEGER(0);
    }
}

NATIVE_METHOD(match_regexp) {
    cList * fields;
    Bool     sensitive=NO, error;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:  sensitive = (Bool) data_true(&args[2]);
        case 2:  INIT_ARG2(STRING)
                 INIT_ARG1(STRING)
                 break;
        default: THROW_NUM_ERROR(ARG_COUNT, "two or three")
    }

    fields = match_regexp(STR2, string_chars(STR1), sensitive, &error);

    if (fields) {
        CLEAN_RETURN_LIST(fields);
    } else {
        if (error == YES) /* we threw an error */
            RETURN_FALSE;
        CLEAN_RETURN_INTEGER(0);
    }
}

NATIVE_METHOD(regexp) {
    cList * fields;
    Bool     sensitive=NO, error;
    DEF_args;

    switch (ARG_COUNT) {
        case 3:  sensitive = (Bool) data_true(&args[2]);
        case 2:  INIT_ARG2(STRING)
                 INIT_ARG1(STRING)
                 break;
        default: THROW_NUM_ERROR(ARG_COUNT, "two or three")
    }

    fields = regexp_matches(STR2, string_chars(STR1), sensitive, &error);
    
    if (fields) {
        CLEAN_RETURN_LIST(fields);
    } else {
        if (error == YES)
            RETURN_FALSE;
        CLEAN_RETURN_INTEGER(0);
    }
}

NATIVE_METHOD(strsed) {
    cStr * out;
    Int        flags=RF_NONE,
               mult=2;
    DEF_args;

    switch (ARG_COUNT) {
        case 5:  CHECK_TYPE(4, INTEGER, "fifth")
                 mult = INT5;
                 if (mult < 0)
                     mult = 2;
                if (mult > 10)
                    THROW((perm_id, "You can only specify a size multiplier of 1-10, sorry!"))
        case 4:  INIT_ARG4(STRING)
                 flags = parse_regfunc_args(string_chars(STR4), flags);
        case 3:  INIT_ARG3(STRING);
                 INIT_ARG2(STRING);
                 INIT_ARG1(STRING);
                 break;
        default: THROW_NUM_ERROR(ARG_COUNT, "three to five")
    }

    if (!(out = strsed(STR2, STR1, STR3, flags, mult)))
        RETURN_FALSE;

    CLEAN_RETURN_STRING(out);
}

/* Encrypt a string. */
NATIVE_METHOD(crypt) {
    cStr * str;

    INIT_1_OR_2_ARGS(STRING, STRING)

    str = strcrypt(STR1, ((argc == 2) ? (STR2) : ((cStr *) NULL)));

    CLEAN_RETURN_STRING(str);
}

NATIVE_METHOD(uppercase) {
    cStr * str;

    INIT_1_ARG(STRING)

    str = string_dup(STR1);

    CLEAN_STACK();
    anticipate_assignment();

    RETURN_STRING(string_uppercase(str));
}

NATIVE_METHOD(lowercase) {
    cStr * str;

    INIT_1_ARG(STRING)

    str = string_dup(STR1);

    CLEAN_STACK();
    anticipate_assignment();

    RETURN_STRING(string_lowercase(str));
}

NATIVE_METHOD(capitalize) {
    char     * s;
    cStr * str;

    INIT_1_ARG(STRING);

    str = string_dup(STR1);

    CLEAN_STACK();
    anticipate_assignment();

    str = string_prep(str, str->start, str->len);
    s = string_chars(str);
    *s = (char) UCASE(*s);

    RETURN_STRING(str);
}

NATIVE_METHOD(strcmp) {
    Int lex;

    INIT_2_ARGS(STRING, STRING)

    lex = strcmp(string_chars(STR1), string_chars(STR2));

    CLEAN_RETURN_INTEGER(lex);
}

NATIVE_METHOD(strfmt) {
    cStr * fmt,
             * out;
    DEF_argc;
    DEF_args;

    if (!argc)
        THROW((numargs_id, "Called with no arguments, requires at least one."))

    if (stack[arg_start].type != STRING)
        THROW((type_id, "First argument (%D) not a string.", &stack[arg_start]))

    fmt = stack[arg_start].u.str;
    args = &stack[arg_start + 1];

    /* if out is NULL, strfmt() threw an error */
    if ((out = strfmt(fmt, args, argc - 1)) == (cStr *) NULL)
        RETURN_FALSE;

    CLEAN_RETURN_STRING(out);
}

NATIVE_METHOD(trim) {
    register char * s;
    char          * ss;
    cStr          * str;
    Int             start;
    Int             len;
    Ident           how;

    INIT_1_OR_2_ARGS(STRING, SYMBOL);

    if (argc == 1)
        how = both_id;
    else
        how = SYM2;

    str = string_dup(STR1);
    ss = string_chars(str);
    len = string_length(str);

    /* they gave us an empty string just return */
    if (!len) {
        CLEAN_RETURN_STRING(str);
    }

    if (how == both_id || how == left_id) {
        for (s=ss; *s == ' '; s++);
        start = s - ss;
    } else {
        start = 0;
    }

    /* if start is len set len to zero and jump past the right side */
    if (start == len) {
        len = 0;
    } else if (how == both_id || how == right_id) {
        for (s=(ss + len-1); *s == ' '; s--);
        len = ((s+1) - start) - ss;
    } else {
        len -= start;
    }

    /* reduce references to 'str' */
    CLEAN_STACK();
    anticipate_assignment();

    /* ok, push it on the stack */
    RETURN_STRING(string_substring(str, start, len));
}

NATIVE_METHOD(split) {
    Int      flags = RF_NONE;
    cList  * list;
    
    INIT_2_OR_3_ARGS(STRING, STRING, STRING);

    if (argc == 3)  
        flags = parse_regfunc_args(string_chars(STR3), flags);

    /* if list is NULL strsplit() threw an error */
    if (!(list = strsplit(STR1, STR2, flags)))
        RETURN_FALSE;

    CLEAN_RETURN_LIST(list);
}

NATIVE_METHOD(word) {
    char * p, * q, * s;
    char * sep = " ";
    cStr * sword = NULL;
    Int    want_word, word, sep_len = 1;

    INIT_2_OR_3_ARGS(STRING, INTEGER, STRING);

    if (argc > 2) {
        sep = string_chars(STR3);
        sep_len = string_length(STR3);
    }

    want_word = INT2;

    if (want_word < 1)
        THROW((type_id, "You cannot index a negative amount."))

    s = p = string_chars(STR1);
    word = 0;
    for (q = strcstr(p, sep); q; q = strcstr(p, sep)) {
        if (q > p) {
            word++;
            if (want_word == word) {
                sword = string_from_chars(p, q - p);
                break;
            }
        }
        p = q + sep_len;
    }

    if (sword == NULL) {
        if (word+1 == want_word)
            sword = string_from_chars(p, string_length(STR1) - (p - s));
        else
            THROW((type_id,"There are not %d words in this string.", want_word))
    }

    CLEAN_RETURN_STRING(sword);
}

/*
// -------------------------------------------------------------------
// Parse the output of an export statement from PROGRESS and other
// similar relational-database export styles.
//
// These systems return fields with a frustrating quote delimitation, quotes
// are escaped within a quote field by doubling them up.  For instance,
// the following would parse as shown:
//
//  "this is a 14"" monitor" 100 "14-Monitor" "" 99
//
//  => ["this is a 14\" monitor", "100", "14-Monitor", "", "99"]
//
// Enjoy -Brandon
*/

#define ADD_WORD(_s_, _len_) {\
    d.u.str = string_from_chars(_s_, _len_); \
    out = list_add(out, &d); \
    string_discard(d.u.str); \
}

NATIVE_METHOD(dbquote_explode) {
    Int             len, sublen;
    cData           d;
    cList         * out;
    char            quote = '"',
                  * sorig;
    register char * s,
                  * p,
                  * t;
            
    INIT_1_OR_2_ARGS(STRING, STRING);

    if (argc == 2) {
       if (string_length(STR2) > 1)
           THROW((type_id, "The second argument must be a single character."))
       quote = string_chars(STR2)[0];
    }
    s = sorig = string_chars(STR1);
    len = string_length(STR1);

    out = list_new(0);
    d.type = STRING;

    forever {
        while (*s && *s == ' ') s++;
        
        p = strchr(s, quote);

        if (p) {
            if (p == s) 
                goto next;

            sublen = p - s;
 
            for (t = (char *) memchr((void *) s, (int) ' ', sublen); t;
                 t = (char *) memchr((void *) s, (int) ' ', sublen))
            {
                if (t > s)
                    ADD_WORD(s, t - s)

                while (*t == ' ') t++;
                sublen -= t - s;
                s = t;
            }
    
            if (*s && sublen)
                ADD_WORD(s, p - s)
 
            next:

            s = ++p;
            p = strchr(s, quote);

            t = p;  

            if (!p || !*p) goto end;

            while (*p && p[1] == quote) {
                p += 2;
                *t = quote;
                t++;
                while (*p && *p != quote)
                    *t++ = *p++;
            }
    
            ADD_WORD(s, t - s)
    
            s = ++p; 
        } else {
            end:

            for (p = strchr(s, ' '); p; p = strchr(s, ' ')) {
                if (p > s)
                    ADD_WORD(s, p - s)
                while (*p == ' ') p++;
                s = p;
            }
 
            if (*s)
                ADD_WORD(s, (sorig + len) - s)
    
            break;
        }       
    }

    CLEAN_RETURN_LIST(out);
}

#undef ADD_WORD
