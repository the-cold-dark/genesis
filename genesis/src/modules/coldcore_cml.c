/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/coldcore_cml.c
// ---
// Miscellaneous operations.
*/

#include "config.h"
#include "defs.h"

#include "y.tab.h"
#include "cdc_types.h"
#include "execute.h"

#define add_delimiter() { \
        p = string_from_chars(s, 1); \
        d.type = STRING; \
        d.u.str = p; \
        list = list_add(list, &d); \
        string_discard(p); \
    }

#define add_str() { \
        if (tlen) { \
            p = string_from_chars(buf, tlen); \
            d.type = STRING; \
            d.u.str = p; \
            list = list_add(list, &d); \
            string_discard(p); \
            buf[0] = NULL; \
            token = buf; \
            tlen = 0; \
        } \
    }

list_t * tokenize_cml(string_t * str) {
    register char * s,
                  * token;
    char            buf[BIGBUF];
    register int    esc, tlen;
    list_t        * list;
    string_t      * p;
    data_t          d;

    s = string_chars(str);
    buf[0] = (char) NULL;
    token = buf;
    tlen = 0;
    esc = 0;
    list = list_new(0);

    for (; *s != (char) NULL; s++) {
        if (esc) {
            esc = 0;

            if (tlen + 1 == BIGBUF - 1)
                add_str();

            tlen++, *token = *s, token++;
        } else {
            switch (*s) {
                case '\\':
                    esc++;
                    break;
                case '{':
                case '}':
                case '[':
                case ']':
                case ':':
                case '=':
                case '"':
                case ' ':
                    add_str();
                    add_delimiter();
                    break;
                default:
                    if (tlen + 1 == BIGBUF - 1)
                        add_str();
                    tlen++, *token = *s, token++;
            }
        }
    }
    add_str();

    return list;
}

void op_tokenize_cml(void) {
    data_t         * args,
                 * elem;
    list_t       * list;
    string_t     * str;

    if (!func_init_1(&args, LIST))
        return;

    list = args[0].u.list;

    str = string_new(0);

    /* Join the list into a single string, denote line breaks */
    for (elem = list_first(list); elem; elem = list_next(list, elem)) {
        if (elem->type != STRING) {
            string_discard(str);
            cthrow(type_id, "Line %d (%D) is not a string.",
                   elem - list_first(list), elem);
            return;
        }
        str = string_add_chars(str, elem->u.str->s, elem->u.str->len);
        str = string_add_chars(str, "{br}", 5);
    }

    /* the old list was just a pointer to args, which gets popped */
    list = tokenize_cml(str);

    pop(1);
    push_list(list);
    list_discard(list);
}
