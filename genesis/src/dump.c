/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: dump.c
// ---
// Routines to handle binary and text database dumps.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "dump.h"
#include "cache.h"
#include "object.h"
#include "log.h"
#include "data.h"
#include "util.h"
#include "execute.h"
#include "grammar.h"
#include "db.h"
#include "ident.h"
#include "lookup.h"

/*
// ------------------------------------------------------------------------
// globals are bad, but they work fine
*/
long   line_count;
FILE * dptr;

internal void text_dump_method(FILE * fp, object_t * obj, char * str, int type);
internal method_t * text_dump_get_method(FILE * fp, object_t *obj, char *name);
internal long get_dbref(char **sptr);

/*
// ------------------------------------------------------------------------
// Binary dump.  This dump must not allocate any memory, since we may be
// performing it under low-memory conditions.
*/
int binary_dump(void) {
    cache_sync();
    return 1;
}

/*
// ------------------------------------------------------------------------
// Text dump.  This dump can allocate memory, and thus shouldn't be used
// as a panic dump for low-memory situations.
*/
int text_dump(void) {
    FILE *fp;
    object_t *obj;
    long name, dbref;

    /* Open the output file. */
    fp = open_scratch_file("textdump.new", "w");
    if (!fp)
        return 0;

    /* Dump the names. */
    name = lookup_first_name();
    while (name != NOT_AN_IDENT) {
        if (!lookup_retrieve_name(name, &dbref))
            panic("Name index is inconsistent.");
        fformat(fp, "name %I %d\n", name, dbref);
        ident_discard(name);
        name = lookup_next_name();
    }

    /* Dump the objects. */
    cur_search++;
    obj = cache_first();
    while (obj) {
        object_text_dump(obj->dbref, fp);
        cache_discard(obj);
        obj = cache_next();
    }

    close_scratch_file(fp);

    if (rename("textdump.new", "textdump") == F_FAILURE)
        return 0;

    return 1;
}

/*
// ------------------------------------------------------------------------
// Text dump.  This dump can allocate memory, and thus shouldn't be used
// as a panic dump for low-memory situations.
*/
#define MATCH(__s, __l) (!strnccmp(line->s, __s, __l) && isspace(line->s[__l]))
#define NEXT_SPACE(__s) { for (; *__s && !isspace(*__s); __s++); }
#define NEXT_WORD(__s)  { for (; isspace(*__s); __s++); }

void text_dump_read(FILE *fp) {
    string_t * line;
    object_t * obj = NULL;
    list_t   * parents = list_new(0);
    data_t     d;
    long       dbref = -1,
               name;
    char     * p,
             * q;
    method_t * method;

    line_count = 0;

    while ((line = fgetstring(fp))) {
        line_count++;

        /* Strip trailing spaces from the line. */
        while (line->len && isspace(line->s[line->len - 1]))
            line->len--;
        line->s[line->len] = 0;

        /* Strip unprintables from the line. */
        for (p = q = line->s; *p; p++) {
            while (*p && !isprint(*p))
                p++;
            *q++ = *p;
        }
        *q = 0;
        line->len = q - line->s;

        if (MATCH("parent", 6)) {
            for (p = line->s + 7; isspace(*p); p++);

            /* Add this parent to the parents list. */
            q = p;
            dbref = get_dbref(&q);
            if (cache_check(dbref)) {
                d.type = DBREF;
                d.u.dbref = dbref;
                parents = list_add(parents, &d);
            } else {
                write_err("Line %d:  Parent %s does not exist.", line_count, p);
            }

        } else if (MATCH("object", 6)) {
            for (p = line->s + 7; isspace(*p); p++);
            q = p;
            dbref = get_dbref(&q);

            /* If the parents list is empty, and this isn't "root", parent it
             * to root. */
            if (!parents->len && dbref != ROOT_DBREF) {
                write_err("Line %d:  Orphan object %s parented to root", line_count, p);
                if (!cache_check(ROOT_DBREF))
                    fail_to_start("Root object not first in text dump.");
                d.type = DBREF;
                d.u.dbref = ROOT_DBREF;
                parents = list_add(parents, &d);
            }

            /* Discard the old object if we had one.  Also see if dbref already
             * exists, and delete it if it does. */
            if (obj)
                cache_discard(obj);
            obj = cache_retrieve(dbref);
            if (obj) {
                obj->dead = 1;
                cache_discard(obj);
            }

            /* Create the new object. */
            obj = object_new(dbref, parents);
            list_discard(parents);
            parents = list_new(0);

        } else if (!strnccmp(line->s, "var", 3) && isspace(line->s[3])) {
            for (p = line->s + 4; isspace(*p); p++);

            /* Get variable owner. */
            dbref = get_dbref(&p);

            /* Skip spaces and get variable name. */
            while (isspace(*p))
                p++;
            name = parse_ident(&p);

            /* Skip spaces and get variable value. */
            while (isspace(*p))
                p++;
            data_from_literal(&d, p);

            if (d.type == -1) {
                write_err("ERROR: class %d name %I unparseable: %s", dbref,
                          name, p);
                d.type = INTEGER;
                d.u.val = 0;
            }

            /* make sure the ancestor is around and create the variable */
            if (cache_check(dbref))
                object_put_var(obj, dbref, name, &d);

            ident_discard(name);
            data_discard(&d);

        } else if (!strnccmp(line->s, "eval", 4)) {
            method = text_dump_get_method(fp, obj, "<eval>");
            if (method) {
                method->name = NOT_AN_IDENT;
                method->object = obj;
                task_method(NULL, obj, method);
                method_discard(method);
            } else {
                write_err("Line %d:  Eval failed", line_count);
            }
        } else if (MATCH("public", 6) || MATCH("method", 6)) {
            text_dump_method(fp, obj, (line->s + 7), MS_PUBLIC);

        } else if (MATCH("private", 7)) {
            text_dump_method(fp, obj, (line->s + 8), MS_PRIVATE);

        } else if (MATCH("protected", 9)) {
            text_dump_method(fp, obj, (line->s + 10), MS_PROTECTED);

        } else if (MATCH("root", 4)) {
            text_dump_method(fp, obj, (line->s + 5), MS_ROOT);

        } else if (MATCH("driver", 6)) {
            text_dump_method(fp, obj, (line->s + 7), MS_DRIVER);

        } else if (!strnccmp(line->s, "name", 4) && isspace(line->s[4])) {
            /* Skip spaces and get name. */
            for (p = line->s + 5; isspace(*p); p++);
            name = parse_ident(&p);

            /* Skip spaces and get dbref. */
            while (isspace(*p))
                p++;
            dbref = atol(p);

            /* Store the name. */
            if (!lookup_store_name(name, dbref))
                fail_to_start("Can't store name--disk full?");

            ident_discard(name);
        }

        string_discard(line);
    }

    if (obj)
        cache_discard(obj);
    list_discard(parents);
}

internal void text_dump_method(FILE     * fp,
                               object_t * obj,
                               char     * str,
                               int        type)
{
    char * p = str;
    int    flags;
    long   name;
    method_t *method;
    int    times=0;

    NEXT_WORD(p);
    name = parse_ident(&p);

    /* step past the current name */
    if (*p == '"') {
        p++;
        for (;;) {
            for (; *p && *p != '"' && *p != '\\'; p++);
            if (*p == '\\') 
                p += 2;
            if (*p == '"') {
                p++;
                break;
            }
            if (!*p)
                break;
        }
    } else {
        NEXT_SPACE(p);
    }

    /* get flags */
    flags = MF_NONE;
    while (*p) {
        times++;
        NEXT_WORD(p);
        if (times > 100)
            exit(0);
        if (!strnccmp(p, "disallow_overrides", 18)) {
            p += 18;
            flags |= MF_NOOVER;
        } else if (!strnccmp(p, "synchronized", 12)) {
            p += 12;
            flags |= MF_SYNC;
        } else if (!strnccmp(p, "locked", 6)) {
            p += 6;
            flags |= MF_LOCK;
        } else if (!strnccmp(p, "native", 6)) {
            p += 6;
            flags |= MF_NATIVE;
        } else {
            NEXT_SPACE(p);
        }
        if (*p == ',')
            p++;
    }

    /* get the method */
    method = text_dump_get_method(fp, obj, ident_name(name));
    if (!method) {
        write_err("Line %d:  Method definition failed", line_count);
    } else {
        method->m_state = type;
        method->m_flags = flags;
        object_add_method(obj, name, method);
        method_discard(method);
    }

    ident_discard(name);
}

/* Get a dbref.  Use some intuition. */
static long get_dbref(char **sptr) {
    char *s = *sptr;
    long dbref, name;
    int result;

    if (isdigit(*s)) {
        /* Looks like the user wants to specify an object number. */
        dbref = atol(s);
        while (isdigit(*++s));
        *sptr = s;
        return dbref;
    } else if (*s == '#') {
        /* Looks like the user really wants to specify an object number. */
        dbref = atol(s + 1);
        while (isdigit(*++s));
        *sptr = s;
        return dbref;
    } else {
        /* It's a name.  If there's a dollar sign (which might be there to make
         * sure that it's not interpreted as a number), skip it. */
        if (*s == '$')
            s++;
        name = parse_ident(&s);
        *sptr = s;
        result = lookup_retrieve_name(name, &dbref);
        ident_discard(name);
        return (result) ? dbref : -1;
    }
}

internal method_t *text_dump_get_method(FILE *fp, object_t *obj, char *name) {
    method_t *method;
    list_t *code, *errors;
    string_t *line;
    data_t d;
    int i;

    code = list_new(0);
    d.type = STRING;
    for (line = fgetstring(fp); line; line = fgetstring(fp)) {
        if (string_length(line) == 1 && *string_chars(line) == '.') {
            /* End of the code.  Compile the method, display any error
             * messages we may have received, and return the method. */
            string_discard(line);
            method = compile(obj, code, &errors);
            list_discard(code);
            for (i = 0; i < errors->len; i++)
                write_err("%l %s: %S", obj->dbref, name, errors->el[i].u.str);
            list_discard(errors);
            return method;
        }

        d.u.str = line;
        code = list_add(code, &d);
        string_discard(line);
    }

    /* We ran out of lines.  This wasn't supposed to happen. */
    write_err("Text dump ended inside method.");
    return NULL;
}

