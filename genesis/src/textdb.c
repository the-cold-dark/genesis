/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: textdb.c
// ---
// text database format handling, used by coldcc.
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
// This is a quick hack for compiling a text-formatted coldc file.
//
// should probably eventually do this with yacc
*/

typedef struct idref_s {
    long dbref;            /* dbref if its an objnum */
    char str[BUF];         /* string name */
    char ren[BUF];         /* ~ renamed name */
} idref_t;

/* globals, because its easier this way */
int        line_count;
object_t * cur_obj;

#define ERR(__s)  (printf("Line %d: %s\n",          line_count, __s))

#define ERRf(__s, __x) { \
        printf("Line %d: ", line_count); \
        printf(__s, __x); \
        fputc(10, stdout); \
    }

#define WARN(__s) (printf("Line %d: Warning: %s\n", line_count, __s))

#define WARNf(__s, __x) { \
        printf("Line %d: Warning: ", line_count); \
        printf(__s, __x); \
        fputc(10, stdout); \
    }

#define DIE(__s) { \
        printf("Line %d: Abort: %s\n", line_count, __s); \
        shutdown(); \
    }

#define DIEf(__s, __x) { \
        printf("Line %d: Abort: ", line_count); \
        printf(__s, __x); \
        fputc(10, stdout); \
        shutdown(); \
    }

#define MATCH(__s, __t, __l) (!strnccmp(__s, __t, __l) && isspace(__s[__l]))
#define NEXT_SPACE(__s) {for (; *__s && !isspace(*__s) && *__s != NULL; __s++);}
#define NEXT_WORD(__s)  {for (; isspace(*__s) && *__s != NULL; __s++);}


/*
// these are access flags, redone because we may want vars to eventually
// have access as well, although it would be different and more restricted
*/

#define N_NEW 1
#define N_OLD 0
#define N_UNDEF -1

#define A_NONE       0x0
#define A_PUBLIC     MS_PUBLIC
#define A_PROTECTED  MS_PROTECTED
#define A_PRIVATE    MS_PRIVATE
#define A_ROOT       MS_ROOT
#define A_DRIVER     MS_DRIVER
#define set_access(__s, __l) { access = __s; s += __l; }
#define no_access(__n) { \
      if (access != A_NONE) { \
          printf("Line %d: invalid access, %s cannot define access.\n", \
                 line_count, __n); \
          shutdown(); \
      } \
    }

/* these are directive flags, for the various directives */
#define D_NONE 0
#define D_VAR 1
#define D_METHOD 2
#define D_EVAL 3
#define set_new(__n) { new = __n; s += 4; }

/* make this do more eventually */
internal void shutdown(void) {
    exit(1);
}

inline int is_valid_id(char * str, int len) {
    while (len--) {
        if (!isalnum(*str) && *str != '_')
            return 0;
        str++;
     }
     return 1;
}

/*
// ------------------------------------------------------------------------
// its small enough lets just do copies, rather than dealing with pointers
*/
#define NOOBJ 0
#define ISOBJ 1

internal idref_t get_idref(char * sp, int len, int isobj) {
    int        num = 0;              /* syntax tracking purposes */
    idref_t    id = {INV_OBJNUM, "", ""};

    if (*sp == '#') {
        sp++;
        num++;
    } else if (*sp == '$') {
        sp++;
    } else if (isobj) {
        DIEf("Invalid object reference \"%s\".", sp);
    }

    if (isdigit(*sp)) {
        if (!num && isobj)
            DIEf("Invalid object reference \"%s\".", sp);
        id.dbref = atol(sp);
    } else {
        char * s = sp;
        char * p = strchr(sp, '~');

        if (p) {
            if (!is_valid_id(s, p - s))
                DIEf("Invalid symbol \"%s\".", sp);
            strncpy(id.str, s, p - s);
            len = len - (p - s + 1);
            if (!is_valid_id(s, len))
                DIEf("Invalid symbol \"%s\".", sp);
            strncpy(id.ren, s, len);
        } else {
            if (!is_valid_id(s, len))
                DIEf("Invalid symbol \"%s\".", sp);
            strncpy(id.str, sp, len);
        }
    }

    return id;
}

internal long parse_to_objnum(idref_t ref) {
    long id, dbref;
    int  result;

    if (ref.str[0] != NULL) {
        id = ident_get(ref.str);
        result = lookup_retrieve_name(id, &dbref);
        ident_discard(id);
        return (result) ? dbref : INV_OBJNUM;
    } else if (ref.dbref != INV_OBJNUM) {
        if (!lookup_retrieve_name(ref.dbref, &dbref))
            return INV_OBJNUM;
        return dbref;
    }

    /* never reached, but it shuts up compilers */
    return INV_OBJNUM;
}

internal object_t * handle_objcmd(char ** sptr, int new) {
    idref_t   obj;
    char     * s = *sptr,
             * p = NULL,
               obj_str[BUF];
    object_t * target = NULL;
    list_t   * parents = list_new(0);
    long       dbref;
    data_t     d;

    /* grab what should be the object number or name */
    p = strchr(s, ':');
    if (p == NULL)
        p = strchr(s, ';');

    if (!p)
        DIEf("Invalid object definition \"%s\".", s);

    /* this gives us a copy for error reporting */
    strncpy(obj_str, s, p - s);

    /* parse the reference */
    obj = get_idref(s, p - s, ISOBJ);
    s = p;

    /* define initial parents */
    if (*s == ':') {
        idref_t parent;
        char     par_str[BUF];
        int      len,
                 more = TRUE;

        /* step past ':' and skip whitespace */
        s++;
        NEXT_WORD(s);

        /* get each parent, look them up */
        while (s != NULL || more) {
            p = strchr(s, ',');
            if (p == NULL) {
                /* they need to use correct syntax */
                if (s[strlen(s) - 1] != ';')
                    DIE("Parse Error, unterminated directive.");
                s[strlen(s) - 1] = NULL;
                strcpy(par_str, s);
                len = strlen(par_str);
                more = FALSE;
            } else {
                strncpy(par_str, s, p - s);
                len = p - s;
            }
            parent = get_idref(par_str, len, ISOBJ);
            dbref = parse_to_objnum(parent);
            if (dbref != INV_OBJNUM && cache_check(dbref)) {
                d.type = DBREF;
                d.u.dbref = dbref;
                parents = list_add(parents, &d);
            } else {
                WARNf("Ignoring undefined parent \"%s\".", par_str);
                WARNf("For object \"%s\".", obj_str);
            }

            /* skip the last word, ',' and whitespace */
            s += (p - s + 1);
            NEXT_WORD(s);
        }
    }

    dbref = parse_to_objnum(obj);
    if (new == N_OLD) {
        if (dbref == INV_OBJNUM) {
            WARNf("old: Object \"%s\" does not exist.", obj_str);
            return NULL;
        } else {
            target = cache_retrieve(dbref);
            if (target == NULL) {
                WARNf("old: Unable to find object \"%s\".", obj_str);
            } else if (dbref == ROOT_DBREF) {
                WARN("old: attempt to destroy $root ignored.");
            } else if (dbref == SYSTEM_DBREF) {
                WARN("old: attempt to destroy $sys ignored.");
            } else {
                ERRf("old: destroying object %s.", obj_str);
                target->dead = 1;
                cache_discard(target);
            }
        }
    } else if (new == N_NEW) {
        if (parents->len == 0 && dbref != ROOT_DBREF)
            DIEf("new: Attempt to define object %s without parents.", obj_str);

        if (dbref != INV_OBJNUM) {
            target = cache_retrieve(dbref);
            if (target) {
                if (dbref == ROOT_DBREF) {
                    DIE("new: Attempt to recreate $root.");
                } else if (dbref == SYSTEM_DBREF) {
                    DIE("new: Attempt to recreate $sys.");
                }
                WARNf("new: destroying existing object %s.", obj_str);
                target->dead = 1;
                cache_discard(target);
            }
        }

        target = object_new(dbref, parents);
    } else {
        target = cache_retrieve(dbref);
        if (!target) {
            WARNf("Creating object \"%s\".", obj_str);
            if (parents->len == 0 && dbref != ROOT_DBREF)
                DIEf("new: Attempt to define object %s without parents.",
                     obj_str);
            target = object_new(dbref, parents);
        }
    }

    /* free up this list */
    list_discard(parents);

    return target;
}

internal void handle_parcmd(char * s, int new) {
    data_t     d;
    char     * p = NULL,
               obj_str[BUF];
    object_t * target = NULL;
    long       dbref;
    list_t   * parents;
    int        num;

    /* grab what should be the object number or name */
    p = strchr(s, ';');
    if (!p)
        DIEf("Invalid object definition \"%s\".", s);

    /* this gives us a copy for error reporting */
    strncpy(obj_str, s, p - s);

    /* parse the reference */
    dbref = parse_to_objnum(get_idref(s, p - s, ISOBJ));

    if (dbref == ROOT_DBREF)
        DIE("Attempt to change $root's parents.");
    if (!cur_obj)
        DIEf("Attempt to %s parent when no object is defined.",
             new ? "add" : "del");

    d.type = DBREF;
    d.u.dbref = dbref;

    parents = list_dup(cur_obj->parents);
    if (new == N_OLD) {
        num = list_search(parents, &d);
        if (num != -1) {
            parents = list_delete(parents, num);
            if (object_change_parents(cur_obj, parents) >= 0)
                WARN("old parent: Oops, something went wrong...");
        }
    } else {
        target = cache_retrieve(dbref);
        if (!target) {
            WARNf("Unable to find object \"%s\" for new parent.", obj_str);
            return;
        }
        cache_discard(target);

        if (list_search(parents, &d) != -1) {
            parents = list_add(parents, &d);
            if (object_change_parents(cur_obj, parents) >= 0)
                WARN("newparent: Oops, something went wrong...");
        }
    }
}

internal void handle_varcmd(char * s, int new, int access) {
    data_t     d;
    char     * p = NULL,
               obj_str[BUF];
    long       definer, var;
    idref_t    name;

    NEXT_WORD(s);

    if (*s == '#' || *s == '$') {
        p = s;
        NEXT_SPACE(p);

        definer = parse_to_objnum(get_idref(s, p - s, ISOBJ));

        if (!cache_check(definer)) {
            WARN("Ignoring variable with invalid parent...");
            return;
        }

        s = p;

    } else {
        if (!cur_obj)
            DIE("var: attempt to define variable without defining object.");
        definer = cur_obj->dbref;
    }

    p = s;
    NEXT_SPACE(p);
    if (*p == ';')
        p--;

    name = get_idref(s, s - p, NOOBJ);
    if (name.ren[0] == NULL)
        DIE("Attempt to rename variable.");
    if (name.str[0] == NULL)
        DIE("Invalid variable name.");

    var = ident_get(name.str);

    /* strip trailing spaces and semi colons */
    while (s[strlen(s) - 1] == ';' || isspace(s[strlen(s) - 1]))
        s[strlen(s) - 1] = NULL;

    if (new == N_OLD) {
        object_delete_var(cur_obj, definer, var);
    } else {
        d.type = -2;
        NEXT_WORD(s);
        if (s == '=') {
            s++;
            NEXT_WORD(s);
            data_from_literal(&d, s);
            if (d.type == -1)
                WARN("data is unparsable, defaulting to '0'.");
        }

        if (d.type < 0) {
            d.type = INTEGER;
            d.u.val = 0;
        }

        object_put_var(cur_obj, definer, var, &d);
    }
}

internal void handle_methcmd(char * s, int new, int access) {
    data_t     d;
    char     * p = NULL,
               obj_str[BUF];
    long       definer, name;
    idref_t    idref;

    NEXT_WORD(s);

    if (*s == '#' || *s == '$') {
        char * tmp;

        for (tmp = s; (*tmp == '.' && *tmp = ' ') && !isspace(*tmp); tmp++);

        definer = parse_to_objnum(get_idref(s, tmp - s, ISOBJ));

        if (!cache_check(definer)) {
            WARN("Ignoring variable with invalid parent...");
            return;
        }

        s = tmp;

        NEXT_WORD(s);

    } else {
        if (!cur_obj)
            DIE("var: attempt to define variable without defining object.");
        definer = cur_obj->dbref;
    }

    p = s;
    NEXT_SPACE(p);
    if (*p == ';')
        p--;

    idref = get_idref(s, s - p, NOOBJ);
    if (idref.ren[0] == NULL)
        DIE("Attempt to rename variable.");
    if (idref.str[0] == NULL)
        DIE("Invalid variable name.");

    name = ident_get(idref.str);

    if 
}

/*
// ------------------------------------------------------------------------
//
*/
void compile_cdc_file(FILE * fp) {
    int        new = 0,
               directive = D_NONE,
               access = A_NONE,
               offset = 0;
    string_t * line;
    char     * p,
             * s,
             * token;
    object_t * obj;

    /* start at line 0 */
    line_count = 0;

    /* use fgetstring because it'll expand until we have the whole line */
    while ((line = fgetstring(fp))) {
        line_count++;

        /* Strip trailing spaces from the line. */
        while (line->len && isspace(line->s[line->len - 1]))
            line->len--;
        line->s[line->len] = NULL;

        /* Strip unprintables from the line. */
        for (p = s = line->s; *p; p++) {
            while (*p && !isprint(*p))
                p++;
            *s++ = *p;
        }
        *s = NULL;
        line->len = s - line->s;

        if (!line->len) {
            string_discard(line);
            continue;
        }

        /* variables, methods and eval will have mutiple lines */
        if (directive) {
            while (line->len && isspace(line->s[line->len - 1])) {
                line->s[line->len - 1] = NULL;
                line->len--;
            }
            sbuf = string_add(sbuf, line);
            s = sbuf->s + offset;
            switch (directive) {
              case D_VAR:
                if (line->s[line->len - 1] == ';') {
                    handle_varcmd(s, new, access);
                    directive = D_NONE;
                }
                break;
              case D_METHOD:
                if (line->s[line->len - 1] == ';' &&
                    line->s[line->len - 2] == '}' &&
                    line->len == 2)
                {
                    handle_methcmd(s, new, access);
                    directive = D_NONE;
                }
                break;
              case D_EVAL:
                if (line->s[line->len - 1] == ';' &&
                    line->s[line->len - 2] == '}' &&
                    line->len == 2)
                {
                    handle_evalcmd(s, new, access);
                    directive = D_NONE;
                }
                break;
            }
            string_discard(sbuf);
            string_discard(line);
            continue;
        }

#define next_token() { \
        token = strtok((char *) NULL, " \t"); \
        if (token == NULL) { \
            WARN("Incomplete directive, ignoring"); \
            string_discard(line); \
            continue; \
        } \
        offset = strlen(token) + 1; \
    }

        token = strtok(line->s, " \t");
        offset = strlen(token) + 1;

        /* old new or who cares? */
        if (MATCH(token, "new", 3))
            set_new(N_NEW)
        else if (MATCH(token, "old", 3))
            set_new(N_OLD)
        else
            new = N_UNDEF;

        next_token();

        /* access? */
        if (MATCH(token, "public", 6))
            set_access(A_PUBLIC, 6)
        else if (MATCH(token, "protected", 9))
            set_access(A_PROTECTED, 9)
        else if (MATCH(token, "private", 7))
            set_access(A_PRIVATE, 7)
        else if (MATCH(token, "root", 4))
            set_access(A_ROOT, 4)
        else if (MATCH(token, "driver", 6))
            set_access(A_DRIVER, 6)
        else
            access = A_NONE;

        next_token();

        if (MATCH(token, "object", 6)) {
            s = (line->s += offset + 6);
            obj = handle_objcmd(s, new);
            if (obj != NULL) {
                if (cur_obj != NULL)
                    cache_discard(cur_obj);
                cur_obj = obj;
            }
        } else if (MATCH(s, "parent", 6)) {
            s = (line->s += offset + 6);
            handle_parcmd(s, new);
        } else if (MATCH(s, "var", 3)) {
            offset += 6;
            directive = D_VAR;
        } else if (MATCH(s, "method", 6)) {
            offset += 6;
            directive = D_METHOD;
        } else if (MATCH(s, "eval", 4)) {
            offset += 5;
            directive = D_EVAL;
        } else if (!MATCH(s, "//", 2)) {
            printf("Line %d: parse error, unknown directive.\n", line_count);
            printf("Line %d: \"%s\"\n", line_count, s);
            shutdown();
        }
    }

#undef next_token()

}

#if DISABLED
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
    method->m_state = type;
    method->m_flags = flags;

    /* add it */
    if (method) {
        object_add_method(obj, name, method);
        method_discard(method);
    } else {
        write_err("Line %d:  Method definition failed", line_count);
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

/*
// redefine this, I dont think you should be able to have
// extraneous chars in an ident
*/
long parse_ident_strict(char **sptr, int len) {
    int        x = len;
    string_t * str;
    char     * s = * sptr;
    long       id;

    /* validate */
    ISVALID(s, x, {return INV_OBJNUM;})

    /* turn it into a string */
    str = string_from_chars(*sptr, len);

    /* get the ident */
    id = ident_get(string_chars(str));
    string_discard(str);
    *sptr = s;
    return id;
}

#endif

