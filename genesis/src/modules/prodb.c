/*
// Full copyright information is available in the file ../doc/CREDITS
//
// SQL interface helpers to a Progress Database, for now its basic, can
// expand upon later for other dbs.
*/

#define NATIVE_MODULE "PROGRESS interface"

#define _prodb_

#include "prodb.h"

/*
// -------------------------------------------------------------------
// Parse the output of an export statement from PROGRESS
//
// Progress returns fields with a frustrating quote delimitation, quotes
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
    Int             len;
    cData          d;
    cList        * out;
    char            quote = '"',
                  * sorig;
    register char * s,
                  * p,
                  * t;
            
    INIT_1_ARG(STRING);
                
    s = sorig = string_chars(STR1);
    len = string_length(STR1);
                    
    out = list_new(0);
    d.type = STRING;
            
    while (1) {
        while (*s && *s == ' ') s++;
        
        p = strchr(s, quote);

        if (p) {
            if (p == s) 
                goto next;

            /* dropping a NULL where the quote is will stop strchr() */
            *p = (char) NULL;
 
            for (t = strchr(s, ' '); t; t = strchr(s, ' ')) {
                if (t > s)
                    ADD_WORD(s, t - s)

                while (*t == ' ') t++;
                s = t;
            }
    
            if (*s)
                ADD_WORD(s, p - s)
 
            next:

            s = ++p;
            p = strchr(s, quote);

            t = p;  

            if (!p || !*p) goto end;

            while (*p && p[1] == quote) {
                p += 2;
                *t = '"';
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

    /* Pop the arguments and push the list onto the stack. */
    CLEAN_RETURN_LIST(out);
}

#undef ADD_WORD
