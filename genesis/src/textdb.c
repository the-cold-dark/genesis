/*
// Full copyright information is available in the file ../doc/CREDITS
//
// text database format handling, used by coldcc.
//
// This has become a beast, quick, put it out of its misery and hack it up
// in YACC
*/

#define TEXTDB_C
#define DEBUG_TEXTDB 0

#include "defs.h"

#include <string.h>
#include <ctype.h>
#include "cdc_db.h"
#include "cdc_pcode.h"
#include "coldcc.h"
#include "util.h"
#include "textdb.h"
#include "moddef.h"
#include "quickhash.h"
#include "cache.h"

/*
// ------------------------------------------------------------------------
// This is a quick hack for compiling a text-formatted coldc file.
//
// should probably eventually do this with yacc
*/

typedef struct idref_s {
    Long objnum;            /* objnum if its an objnum */
    char str[BUF];         /* string name */
    Int  len;
    Int  err;
} idref_t;

/* globals, because its easier this way */
Int        use_natives;
Long       line_count;
Long       method_start;
Obj * cur_obj;
static Hash * dump_hash;
extern Bool print_objs;
extern Bool print_invalid;
extern Bool print_warn;

#define ERR(__s)  (printf("\rLine %ld: %s\n", (long) line_count, __s))

#define ERRf(__s, __x) { \
        printf("\rLine %ld: ", (long) line_count); \
        printf(__s, __x); \
        fputc('\n', stdout); \
	fflush(stdout); \
    }

#define WARN(_printf_) { \
        if (print_warn) { \
            printf("\rLine %ld: WARNING: ", (long) line_count); \
            printf _printf_; \
            fputc('\n', stdout); \
	    fflush(stdout); \
        } \
    }

#define DIE(__s) { \
        printf("\rLine %ld: ERROR: %s\n", (long) line_count, __s); \
	fflush(stdout); \
        shutdown_coldcc(); \
    }

#define DIEf(__fmt, __arg) { \
        printf("\rLine %ld: ERROR: ", (long) line_count); \
        printf(__fmt, __arg); \
        fputc('\n', stdout); \
	fflush(stdout); \
        shutdown_coldcc(); \
    }

/* Dancer: This is more portable than the pointer arithmetic
           that it replaces.  This should work on all boxes */
#define COPY(__buf, __s1, __s2) { \
        char s0; \
        s0=*__s2; \
        *__s2='\0'; \
          strcpy(__buf, __s1); \
        *__s2=s0; \
    }

#define MATCH(__s, __t, __l) (!strnccmp(__s, __t, __l) && isspace(__s[__l]))
#define NEXT_SPACE(__s) {for (; *__s && !isspace(*__s) && *__s != (char) NULL; __s++);}
#define NEXT_WORD(__s)  {for (; isspace(*__s) && *__s != (char) NULL; __s++);}


/*
// ------------------------------------------------------------------------
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
#define A_FROB       MS_FROB
#define A_ROOT       MS_ROOT
#define A_DRIVER     MS_DRIVER

/*
// ------------------------------------------------------------------------
*/
static Method * get_method(FILE * fp, Obj * obj, char * name);
char * strchop(char * str, Int len);
static void print_dbref(Obj * obj, cObjnum objnum, FILE * fp, Bool objnames);
void blank_and_print_obj(char * what, Float percent_done, Obj * obj);

typedef struct holder_s holder_t;

/* native holder */
typedef struct nh_s nh_t;

struct nh_s {
    Long    objnum;
    Ident   native;     /* the native name */
    Ident   method;     /* if it has been renamed, this is the method name */
    Int     valid;
    nh_t  * next;
};

struct holder_s {
    Long       objnum;
    cStr * str;
    holder_t * next;
};

static holder_t * holders = NULL;

nh_t * nhs = NULL;

static Int add_objname(char * str, Long objnum, Int skip_lookup) {
    Ident   id = ident_get(str);
    Obj   * obj = NULL;
    Long    num = INV_OBJNUM;

    if (!skip_lookup && lookup_retrieve_name(id, &num) && num != objnum) {
        WARN(("Attempt to rebind existing objname $%s (#%li)",
               str, (long) num));
        ident_discard(id);
        return 0;
    }

    /* the object doesn't exist yet, so lets add the name to the db,
       with the number, and keep it in a holder stack so we can set
       the name on the object after it is defined */
    obj = cache_retrieve(objnum);
    if (!obj) {
        holder_t * holder = (holder_t *) malloc(sizeof(holder_t));

        lookup_store_name(id, objnum);

        holder->objnum = objnum;
        holder->str = string_dup(ident_name_str(id));
        holder->next = holders;
        holders = holder;
    } else {
        if (num == objnum)
            obj->objname = ident_dup(id);
        else
            object_set_objname(obj, id);
        cache_discard(obj);
    }

    ident_discard(id);

    return 1;
}

/* here because data_from_literal() calls it, and genesis wants to handle
   it differently -- bad, will fix eventually */
cObjnum get_object_name(Ident id) {
    cObjnum num;

    if (!lookup_retrieve_name(id, &num)) {
        num = db_top++;
        add_objname(ident_name(id), num, TRUE);
    }

    return num;
}


static void cleanup_holders(void) {
    holder_t * holder = holders,
             * old = NULL;
    Long       objnum;
    Obj      * obj;
    Ident      id;
    cData      key;

    key.type = OBJNUM;

    while (holder != NULL) {
        key.u.objnum = holder->objnum;
        if (hash_find(dump_hash, &key) == F_FAILURE) {
            id = ident_get_string(holder->str);
            if (!lookup_retrieve_name(id, &objnum)) {
                if (print_warn)
                    printf("\rWARNING: Name $%s for object #%d disapppeared.\n",
                           ident_name(id), (int) objnum);
            } else if (objnum != holder->objnum) {
                if (print_warn)
                   printf("\rWARNING: Name $%s is no longer bound to object #%d.\n",
                          ident_name(id), (int) objnum);
            } else {
                obj = cache_retrieve(holder->objnum);
                if (obj) {
                    if (obj->objname == NOT_AN_IDENT) {
                        cache_dirty_object(obj);
                        obj->objname = ident_dup(id);
                    }
                    cache_discard(obj);
                } else {
                    if (print_warn)
                        printf("\rWARNING: Object $%s (#%d) was never defined.\n",
                               ident_name(id), (int) objnum);
                    lookup_remove_name(id);
                }
            }
            ident_discard(id);
        }

        string_discard(holder->str);
        old = holder;
        holder = holder->next;
        free(old);
    }
}

#ifndef ONLY_PARSE_TEXTDB
/* only call with a method which declars a MF_NATIVE flag */
/* holders are redundant, but it lets us keep track of methods defined
   native, but which are not */
static nh_t * find_defined_native_method(cObjnum objnum, Ident name) {
    nh_t * nhp;

    for (nhp = nhs; nhp != (nh_t *) NULL; nhp = nhp->next) {
        if (nhp->native == name) {
            if (nhp->objnum == objnum)
                return nhp;
        }
    }

    return (nh_t *) NULL;
}

static void remember_native(Method * method) {
    nh_t  * nh;

    nh = find_defined_native_method(method->object->objnum, method->name);
    if (nh != (nh_t *) NULL) {
        fformat(stdout,
            "\rLine %l: ERROR: %O.%s() overrides existing native definition.\n",
            line_count, nh->objnum, ident_name(nh->native));
        shutdown_coldcc();
    }

    nh = (nh_t *) malloc(sizeof(nh_t));

    nh->objnum = method->object->objnum;
    nh->valid = 0;
    nh->next = nhs;
    nhs = nh;
    nh->native = ident_dup(method->name);
    nh->method = NOT_AN_IDENT;
}

static void frob_n_print_errstr(char * err, char * name, cObjnum objnum);

static Int find_native_method(Long object, Long method_name) {
    Ident      oname;
    Ident      mname;
    native_t * native;
    Int x;

    for (x = 0; x < NATIVE_LAST; x++) {
        native = &natives[x];

        if ((strlen(native->bindobj) == 0) || (strlen(native->name) == 0))
            continue;

        if (native->bindobj_ident == NOT_AN_IDENT) {
            if (strlen(native->bindobj) == 0)
                continue;
            oname = ident_get(native->bindobj);
            if (oname == NOT_AN_IDENT)
                continue;
            native->bindobj_ident = oname;
        } else
            oname = native->bindobj_ident;

        if (native->name_ident == NOT_AN_IDENT) {
            if (strlen(native->name) == 0)
                continue;
            mname = ident_get(native->name);
            if (mname == NOT_AN_IDENT)
                continue;
            native->name_ident = mname;
        }
            mname = native->name_ident;

        if (oname != object)
            continue;

        if (mname ==  method_name)
            return x;
    }

    return -1;
}

static void verify_native_methods(void) {
    Ident      mname;
    Ident      name;
    Obj      * obj;
    Method   * method = NULL;
    cObjnum    objnum;
    cList    * errors;
    cList    * code = list_new(0);
    native_t * native;
    register   Int x;
    nh_t     * nh = (nh_t *) NULL;

    /* check the methods we know about */
    for (x=0; x < NATIVE_LAST; x++) {
        native = &natives[x];

        /* if they didn't define it right, ignore it */
        if ((strlen(native->bindobj) == 0) || (strlen(native->name) == 0))  
            continue;
  
        /* get the object name */
        name = ident_get(native->bindobj);
        if (name == NOT_AN_IDENT)
            continue;

        /* find the object */
        objnum = INV_OBJNUM;
        lookup_retrieve_name(name, &objnum);
        ident_discard(name);
  
        /* die? */
        if (objnum == INV_OBJNUM) {
            if (print_warn)
                printf("\rWARNING: Unable to find object for native $%s.%s()\n",
                       native->bindobj, native->name);
            continue;
        }

        /* pull the object or die if we cant */
        obj = cache_retrieve(objnum);
        if (!obj) {
            if (print_warn)
                printf("\rWARNING: Unable to retrieve object #%li ($%s)\n",
                       (long) objnum, native->bindobj);
            continue;
        }

        /* is the name correct? */
        name = ident_get(native->name);
        if (name == NOT_AN_IDENT) {
            if (print_warn)
                fformat(stdout,
                 "\rWARNING: Invalid name \"%s\" for native method on \"%O\"\n",
                   native->name, obj->objnum);
            cache_discard(obj);
            continue;
        }

        /* get a copy to reference the actual method name */
        mname = ident_dup(name);

        /* see if we have defined it already */
        nh = find_defined_native_method(objnum, name);

        /* If so, see if we need to change the method name appropriately */
        if (nh != (nh_t *) NULL) {
            if (nh->method != NOT_AN_IDENT) {
                ident_discard(mname);
                mname = ident_dup(nh->method);
            }
        }

        /* now find it on the object, use 'mname' as the method name */
        method = object_find_method(objnum, mname, FROB_ANY);

        /* it does not exist, compile an empty method */
        if (method == NULL) {
            method = compile(obj, code, &errors);
            method->native = x;
            method->m_flags |= MF_NATIVE;

            object_add_method(obj, mname, method);

            if (nh != (nh_t *) NULL)
                nh->valid = 1;
            if (errors != NULL)
                list_discard(errors);

            method_discard(method);

        /* it was prototyped, set the native structure pointer and
           mark the object as dirty */
        } else {
            if (!(method->m_flags & MF_NATIVE) &&
                 use_natives != FORCE_NATIVES)
            {
                if (print_warn)
                    fformat(stdout, "\rWARNING: method definition %O.%s() overrides native method.\n", obj->objnum, ident_name(mname));
            } else {
		cache_dirty_object(obj);
                method->native = x;
                method->m_flags |= MF_NATIVE;

                if (nh != (nh_t *) NULL)
                    nh->valid = 1;
            }
            cache_discard(method->object);
        }

        ident_discard(mname);
        ident_discard(name);
        cache_discard(obj);
    }

    list_discard(code);

    /* now cleanup method holders */
    while (nhs != (nh_t *) NULL) {
        nh = nhs;
        nhs = nh->next;

        if (nh->method != NOT_AN_IDENT) {
            name = nh->method;
            ident_discard(nh->native);
        } else {
            name = nh->native;
        }

        if (nh->valid) {
            ident_discard(name);
        } else {
            /* remove the native array designator from the method,
               but not the native mask */
            cur_obj = cache_retrieve(objnum);
            if (print_warn)
                printf("\rWARNING: No native definition for method .%s()\n",
                       ident_name(name));
            if (cur_obj) {
                method = object_find_method_local(cur_obj, name, FROB_ANY);
                if (method) {
		    cache_dirty_object(cur_obj);
                    method->native = -1;
                }
                cache_discard(cur_obj);
            }
            ident_discard(name);
        }

        free(nh);
    }
}
#endif

/*
// ------------------------------------------------------------------------
// its small enough lets just do copies, rather than dealing with pointers
*/
#define NOOBJ 0
#define ISOBJ 1

static Int get_idref(char * sp, idref_t * id, Int isobj) {
    Int    x;
    char * end;

    id->objnum = INV_OBJNUM;
    id->str[0] = 0;
    id->len    = 0;

    if (!sp[0]) {
        id->err = 1;
        return 0;
    }

    id->err = 0;

    /* special case objnums, drop out of need be */
    if (isobj && sp[0] == '#') {
        if (isdigit(sp[1]) || (sp[1] == '-' && isdigit(sp[2]))) {
            id->objnum = strtol(&sp[1], &end, 10);
            return (end - sp);
        } else {
            DIEf("Invalid objnum \"%s\".", sp)
        }
    }

    /* get just the symbol */
    for (x = 0;
         sp[x] != (char) NULL && (isalnum(sp[x]) || sp[x] == '_' || sp[x] == '$');
         x++);

    if (sp[0] == '$') {
        if (!isobj)
            DIEf("Invalid symbol '%s.", sp)
        strncpy(id->str, &sp[1], x-1);
        id->str[x-1] = 0;
        id->len = x-1;
    }
    else
    {
        strncpy(id->str, sp, x);
        id->str[x] = 0;
        id->len = x;
    }

    return x;
}

#ifndef ONLY_PARSE_TEXTDB
/*
// ------------------------------------------------------------------------
*/
static Long parse_to_objnum(idref_t *ref) {
    Long id,
         objnum = 0;
    Int  result;

    if (ref->str[0] != (char) NULL) {
        if (!strncmp(ref->str, "root", 4) && ref->len == 4)
            return 1;
        else if (!strncmp(ref->str, "sys", 3) && ref->len == 3)
            return 0;

        id = ident_get_length(ref->str, ref->len);
        result = lookup_retrieve_name(id, &objnum);
        ident_discard(id);

        return (result) ? objnum : INV_OBJNUM;
    }

    return ref->objnum;
}
#endif

/*
// ------------------------------------------------------------------------
*/

static Obj * handle_objcmd(char * line, char * s, Int new) {
    idref_t   obj;
    char    * p = (char) NULL,
              obj_str[BUF];
#ifndef ONLY_PARSE_TEXTDB
    Obj     * target = NULL;
    cList   * parents = list_new(1); /* will always have a least one parent */
    Long      objnum;
    cData     d;
#endif

    /* grab what should be the object number or name */
    p = strchr(s, ':');
    if (p == NULL) {
        p = strchr(s, ';');
        if (p == NULL) {
            ERR("Invalid directive termination:");
            DIEf("\"%s\"", line);
        }
    }

    /* this gives us a copy for error reporting */
    COPY(obj_str, s, p);

    /* parse the reference */
    s += get_idref(obj_str, &obj, ISOBJ);

    /* define initial parents */
    if (*s == ':') {
        idref_t parent;
        char     par_str[BUF];
        Int      len,
                 more = TRUE,
                 slen;

        /* step past ':' and skip whitespace */
        s++;
        NEXT_WORD(s);

        /* get each parent, look them up */
        while ((more && *s != (char) NULL) && running) {
            p = strchr(s, ',');
            if (p == NULL) {
                /* we may be at the end of the line.. */
                slen = strlen(s);
                if (s[slen - 1] != ';')
                    DIE("Parse Error, unterminated directive.")
                s[slen - 1] = (char) NULL;
                strcpy(par_str, s);
                len = strlen(par_str);
                more = FALSE;
            } else {
                strncpy(par_str, s, p - s);
                par_str[p - s] = (char) NULL;
                len = p - s;
            }
            get_idref(par_str, &parent, ISOBJ);
#ifndef ONLY_PARSE_TEXTDB
            objnum = parse_to_objnum(&parent);
            if (VALID_OBJECT(objnum)) {
                d.type = OBJNUM;
                d.u.objnum = objnum;
                parents = list_add(parents, &d);
            } else {
                if (objnum >= 0) {
                    WARN(("Ignoring undefined parent \"%s\".", par_str));
                } else {
                    WARN(("Ignoring invalid parent \"%s\".", par_str));
                }
                WARN(("For object \"%s\".", obj_str));
            }
#endif

            /* skip the last word, ',' and whitespace */
            if (more) {
                s += (p - s + 1);
                NEXT_WORD(s);
            }
        }
    }

#ifndef ONLY_PARSE_TEXTDB
    objnum = parse_to_objnum(&obj);

    if (new == N_OLD) {
        if (objnum < 0) {
            WARN(("old: Invalid Object \"%s\"", obj_str));
            list_discard(parents);
            return NULL;
        } else {
            target = cache_retrieve(objnum);
            if (!target) {
                WARN(("old: Unable to find object \"%s\".", obj_str));
            } else if (objnum == ROOT_OBJNUM) {
                WARN(("old: attempt to destroy $root ignored."));
            } else if (objnum == SYSTEM_OBJNUM) {
                WARN(("old: attempt to destroy $sys ignored."));
            } else {
                ERRf("old: destroying object %s.", obj_str);
		cache_dirty_object(target);
                target->dead = 1;
                cache_discard(target);
                target = NULL;
                list_discard(parents);
                return NULL;
            }
        }
    } else if (new == N_NEW) {
        if (!parents->len && objnum != ROOT_OBJNUM)
            DIEf("new: Attempt to define object %s without parents.", obj_str);

        if (objnum == ROOT_OBJNUM || objnum == SYSTEM_OBJNUM) {
            WARN(("new: Attempt to recreate %s ignored.", obj_str));

            /* $root and $sys should ALWAYS exist */
            target = cache_retrieve(objnum);
        } else {
	    // XXX: this code makes a bad assumption that is no longer universally true
	    //      if target is a parent with children in the cache, its refcount won't
	    //      be 1 and so the discard won't actually get rid of the object and the
	    //      object_new is going to cause problems because it assumes that the
	    //      objnum doesn't exist.
	    //
	    //      The code that makes this unsafe isn't checked in, so at the moment its
	    //      still safe
	    //
            if ((target = cache_retrieve(objnum))) {
                WARN(("new: destroying existing object %s.", obj_str));
		cache_dirty_object(target);
                target->dead = 1;
                cache_discard(target);
                target = NULL;
            }
            target = object_new(objnum, parents);
        }
    } else {
        target = cache_retrieve(objnum);

        if (!target) {
            WARN(("Creating object \"%s\".", obj_str));
            if (parents->len == 0 && objnum != ROOT_OBJNUM)
                DIEf("Attempt to define object %s without parents.", obj_str);
            target = object_new(objnum, parents);
            if (!target) {
                DIEf("ABORT, unable to create object #%li", (long) objnum);
            }
        }

    }

    d.type = OBJNUM;
    d.u.objnum = objnum;
    dump_hash = hash_add(dump_hash, &d);

    /* if we should, add the name.  If it already has one, we just replace it.*/
    if (objnum != ROOT_OBJNUM && objnum != SYSTEM_OBJNUM) {
        if (obj.str[0] != (char) NULL)
            add_objname(obj.str, target->objnum, objnum == INV_OBJNUM);
    }

    /* free up this list */
    list_discard(parents);

    return target;
#else
    return NULL;
#endif
}

/*
// ------------------------------------------------------------------------
*/
static void handle_parcmd(char * s, Int new) {
#ifndef ONLY_PARSE_TEXTDB
    cData      d;
    char       obj_str[BUF];
    Long       objnum;
    Obj      * target = NULL;
    Int        num;
    cList    * parents;
#endif

    char     * p = NULL;
    Int        len;
    idref_t    id;

    /* parse the reference */
    len = get_idref(s, &id, ISOBJ);
    p = s + len;
    NEXT_SPACE(p);
    if (*p != ';' || !len)
        DIEf("Invalid object definition \"%s\".", s);

#ifndef ONLY_PARSE_TEXTDB
    objnum = parse_to_objnum(&id);

    if (objnum == ROOT_OBJNUM)
        DIE("Attempt to change $root's parents.");
    if (!cur_obj)
        DIEf("Attempt to %s parent when no object is defined.",
             new ? "add" : "del");

    d.type = OBJNUM;
    d.u.objnum = objnum;

    parents = list_dup(cur_obj->parents);
    if (new == N_OLD) {
        num = list_search(parents, &d);
        if (num != -1) {
            parents = list_delete(parents, num);
            if (object_change_parents(cur_obj, parents) >= 0)
                WARN(("old parent: Oops, something went wrong..."));
        }
    } else {
        target = cache_retrieve(objnum);
        if (!target) {
            WARN(("Unable to find object \"%s\" for new parent.", obj_str));
            return;
        }
        cache_discard(target);

        if (list_search(parents, &d) != -1) {
            parents = list_add(parents, &d);
            if (object_change_parents(cur_obj, parents) >= 0)
                WARN(("newparent: Oops, something went wrong..."));
        }
    }
#endif
}

/*
// ------------------------------------------------------------------------
*/
static void handle_namecmd(char * line, char * s, Int new) {
    char       name[BUF];
    char     * p;
#ifndef ONLY_PARSE_TEXTDB
    Long       num, other;
    Ident      id;
#endif

    /* bump if they have a '$' in the name */
    if (*s == '$')
        s++;

    p = s;

    /* skip past the name */
    for (; *p && !isspace(*p) && *p != (char) NULL && *p != ';'; p++);

    /* copy the name */
    COPY(name, s, p);

#ifndef ONLY_PARSE_TEXTDB
    /* see if it exists */
    id = ident_get(name);
    if (lookup_retrieve_name(id, &other)) {
        ident_discard(id);
        WARN(("objname $%s is already bound to objnum #%li",name,(long) other));
        return;
    }
    ident_discard(id);

    /* lets see if there is a objnum association, or if we should pick one */
    for (; isspace(*p) && *p != (char) NULL; p++);

    if (*p != ';') {
        if (!p) {
            ERR("Abnormal termination of name directive:");
            DIEf("\"%s\"", line);
        }

        if (*p == '#')
            p++;

        num = (Long) atoi(p);

        if (!num && *p != '0') {
            ERR("Invalid object number association:");
            DIEf("\"%s\"", line);
        }
    } else {
        num = db_top++;
    }

    add_objname(name, num, TRUE);
#endif
}

/*
// ------------------------------------------------------------------------
*/
static void handle_varcmd(char * line, char * s, Int new, Int access) {
#ifndef ONLY_PARSE_TEXTDB
    static cObjnum last_definer = INV_OBJNUM;
    static Int rc_check;

    Long       definer, var;
    Obj      * def;
    Bool       result;
    char     * var_name;
#endif
    Int        slen;
    cData      d;
    char     * p = s;
    idref_t    name;

    if (*s == '#' || *s == '$') {
        s += get_idref(s, &name, ISOBJ);
#ifndef ONLY_PARSE_TEXTDB
        definer = parse_to_objnum(&name);

	if (last_definer != definer) {
	    rc_check = cache_check(definer);
	    last_definer = definer;
	}

	if (!rc_check) {
            WARN(("Ignoring object variable with invalid parent:"));
            if (strlen(line) > 55) {
                line[50] = line[51] = line[52] = '.';
                line[53] = (char) NULL;
            }
            WARN(("\"%s\"", line));
            return;
        }

	if (!object_has_ancestor(cur_obj->objnum, definer)) {
            WARN(("Ignoring object variable with no ancestor:"));
            if (strlen(line) > 55) {
                line[50] = line[51] = line[52] = '.';
                line[53] = (char) NULL;
            }
            WARN(("\"%s\"", line));
            return;
        }

        NEXT_WORD(s);
    } else {
        if (!cur_obj)
            DIE("var: attempt to define object variable without defining object.");
        definer = cur_obj->objnum;
#else
        NEXT_WORD(s)
#endif
    }

    /* strip trailing spaces and semi colons */
    slen = strlen(s) - 1;
    while (slen >= 0 && (s[slen] == ';' || isspace(s[slen]))) {
        s[slen] = (char) NULL;
        --slen;
    }

    s += get_idref(s, &name, NOOBJ);

    if (name.str[0] == (char) NULL)
        DIEf("Invalid variable name \"%s\"", p);

#ifndef ONLY_PARSE_TEXTDB
    var = ident_get(name.str);
#endif

    if (new == N_OLD) {
#ifndef ONLY_PARSE_TEXTDB
        def = cache_retrieve(definer);

        if (!def)
            DIE("Abnormal disappearance of object.");

        /* axe the variable */
        object_delete_var(cur_obj, def, var);
        cache_discard(def);
#endif
    } else {
        d.type = -2;

        /* skip the current 'word' until we hit a space or a '=' */
        for (; *s && !isspace(*s) && *s != (char) NULL && *s != '='; s++);

        /* incase we hit a space and not a '=', bump it up to the next word */
        NEXT_WORD(s);

        if (*s == '=') {
            s++;
            NEXT_WORD(s);
            s = data_from_literal(&d, s);
#ifndef ONLY_PARSE_TEXTDB
            if (d.type == -1) {
                if (print_warn) {
                    printf("\rLine %ld: WARNING: invalid data for variable ", (long) line_count);
                    print_dbref(cur_obj, cur_obj->objnum, stdout, TRUE);
                    if (cur_obj->objnum!=definer && (def=cache_retrieve(definer))) {
                        fputc('<', stdout);
                        print_dbref(def, def->objnum, stdout, TRUE);
                        fputc('>', stdout);
                        cache_discard(def);
                    }
                    printf(",%s:\nLine %ld: WARNING: data: %s\nLine %ld: WARNING: Defaulting value to ZERO ('0').\n",
                           ident_name(var), (long) line_count, strchop(s, 50), (long) line_count);
	            fflush(stdout);
                }
            }
#endif
            if (*s && *s != ';')
                NEXT_WORD(s);
            if (*s && *s != ';') {
                ERR("DATA is not terminated correctly:");
                DIEf("=> %s\n", s);
            }
        }

#ifndef ONLY_PARSE_TEXTDB
        if (d.type < 0) {
            d.type = INTEGER;
            d.u.val = 0;
        }

        result = object_put_var(cur_obj, definer, var, &d);
        data_discard(&d);

        if (!result) {
            var_name = ident_name(var);
            if (print_warn) {
                printf("\rLine %ld: WARNING: Variable ", (long) line_count);
                print_dbref(cur_obj, cur_obj->objnum, stdout, TRUE);
                if (cur_obj->objnum != definer && (def = cache_retrieve(definer))) {
                    fputc('<', stdout);
                    print_dbref(def, definer, stdout, TRUE);
                    fputc('>', stdout);
                    cache_discard(def);
                }
                printf(",%s no longer defined.\n", var_name);
            }
        }
#endif
    }
}

/*
// ------------------------------------------------------------------------
*/
static void handle_evalcmd(FILE * fp, char * s, Int new, Int access) {
    Method * method;

#ifndef ONLY_PARSE_TEXTDB
    Long       name;

    /* set the name as 'coldcc_eval */
    name   = ident_get("coldcc_eval");

    /* grab the code */
    method = get_method(fp, cur_obj, ident_name(name));

    /* die if its invalid */
    if (!method)
        DIE("Method definition failed");

    /* run it */
    method->name = name;
    method->object = cur_obj;
    vm_method(cur_obj, method);

    /* toss it */
    method_discard(method);
    ident_discard(name);
#else
    method = get_method(fp, cur_obj, NULL);
#endif
}

/*
// ------------------------------------------------------------------------
*/
static Int get_method_name(char * s, idref_t * id) {
    Int    count = 0, x;
    char * p;

    if (*s == '.')
        s++, count++;

    for (x=0, p=s; *p != (char) NULL; x++, p++) {
        if (isalnum(*p) || *p == '_')
            continue;
        break;
    }

    count += x;
    strncpy(id->str, s, x);
    id->str[x] = (char) NULL;

    return count;
}

static void handle_bind_nativecmd(FILE * fp, char * s) {
    idref_t    nat;
    idref_t    meth;
#ifndef ONLY_PARSE_TEXTDB
    Ident      inat, imeth;
    nh_t     * n = (nh_t *) NULL;
#endif

    s += get_method_name(s, &nat);

    if (*s == '(')
        s+=2;

    NEXT_WORD(s);

    s += get_method_name(s, &meth);
   
#ifndef ONLY_PARSE_TEXTDB
    if (nat.str[0] == (char) NULL || meth.str[0] == (char) NULL)
        DIE("Invalid method name in bind_native directive.\n")

    inat = ident_get(nat.str);
    imeth = ident_get(meth.str);

    n = find_defined_native_method(cur_obj->objnum, imeth);
    if (n == (nh_t *) NULL)
        DIE("Attempt to bind_native to method which is not native.\n")

    /* if they've already bound it, we have precedence */
    if (n->method != NOT_AN_IDENT)
        ident_discard(n->method);
    ident_discard(n->native);

    /* remember the new method we are bound to */
    n->native = ident_dup(inat);
    n->method = ident_dup(imeth);

    ident_discard(inat);
    ident_discard(imeth);
#endif
}

static void handle_methcmd(FILE * fp, char * s, Int new, Int access) {
    char     * p = NULL;
#ifndef ONLY_PARSE_TEXTDB
    cObjnum   definer;
    Ident      name;
#endif
    idref_t    id = {INV_OBJNUM, "", 0};
    Method * method;
    Obj * obj;
    Int        flags = MF_NONE;

    NEXT_WORD(s);

    if (*s == '#' || *s == '$') {
        s += get_idref(s, &id, ISOBJ);
        if (id.err)
            DIE("Invalid object \"$\"")

#ifndef ONLY_PARSE_TEXTDB
        /* parse the parent.. */
        definer = parse_to_objnum(&id);

        /* make sure it exists, and not just as a name */
        if (!cache_check(definer))
            DIE("method defined with invalid parent...")
    } else {
        if (!cur_obj)
            DIE("attempt to define method without defining object.");
        definer = cur_obj->objnum;
#endif
    }

    s += get_method_name(s, &id);

    if (id.str[0] == (char) NULL)
        DIE("No method name.");

#ifndef ONLY_PARSE_TEXTDB
    name = ident_get(id.str);
#endif

    /* see if any flags are set */
    if ((p = strchr(s, ':')) != NULL) {
        p++;

        while (*p != (char) NULL && running) {
            NEXT_WORD(p);   
            if (!strnccmp(p, "nooverride", 10)) {
                p += 10;
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
            } else if (!strnccmp(p, "forked", 6)) {
                p += 6;
                flags |= MF_FORK;
            } else if (*p == '{' || *p == ';') {
                break;
            } else {
                char ebuf[BUF];

                s = p;
                NEXT_SPACE(s);
                if (*s == ',')
                    s--;
                COPY(ebuf, p, s);

                WARN(("Unknown flag: \"%s\".", ebuf));

                p = s;
            }
            if (*p == ',')
                p++;
        }
    } else {
        if ((p = strchr(s, ';')) == NULL &&
            (p = strchr(s, '{')) == NULL)
            DIE("Un-terminted method definition.")
    }

#ifndef ONLY_PARSE_TEXTDB
    obj = cache_retrieve(definer);

    if (!obj)
        DIE("Abnormal disappearance of object.");
#endif

    if (*p != ';') {
#ifndef ONLY_PARSE_TEXTDB
        /* get the method */
        method = get_method(fp, obj, ident_name(name));
    } else {
        cList * code = list_new(0);
        cList * errors;

        method = compile(obj, code, &errors);

        list_discard(code);
        if (errors != NULL)
            list_discard(errors);
#else
        method = get_method(fp, obj, NULL);
#endif
    }

#ifndef ONLY_PARSE_TEXTDB
    if (!method)
        DIE("Method definition failed");

    method->m_access = access;
    method->m_flags = flags;

    object_add_method(obj, name, method);

    if (method->m_flags & MF_NATIVE) {
        Int x;

        x = find_native_method(obj->objname, name);
        if (x != -1)
            method->native = x;

        remember_native(method);
    }

    method_discard(method);

    /* free up the remaining resources */
    ident_discard(name);
    cache_discard(obj);
#endif
}

#ifndef ONLY_PARSE_TEXTDB
/*
// ------------------------------------------------------------------------
*/
static void frob_n_print_errstr(char * err, char * name, cObjnum objnum) {
    Int        line = 0;
    cStr * str;

    if (strncmp("Line ", err, 5) == 0) {
        err += 5;
        while (isdigit(*err))
            line = line * 10 + *err++ - '0';
        err += 2;
    }

    str = format("\rLine %l: [line %d in %O.%s()]: %s",
                 method_start + line,
                 line,
                 objnum,
                 name,
                 err);

    write_err("%s", str->s);

    string_discard(str);
}
#endif

static Method * get_method(FILE * fp, Obj * obj, char * name) {
    Method * method;
    cStr   * line;
#ifndef ONLY_PARSE_TEXTDB
    cList  * code;
    cList  * errors;
    cData    d;
    Int      i;

    code = list_new(0);
    d.type = STRING;
#endif

    /* used in printing method errs */
    method_start = line_count;
    for (line = fgetstring(fp); line && running; line = fgetstring(fp)) {
        line_count++;

        /* hack for determining the end of a method */
        if (line->len == 2 && line->s[0] == '}' && line->s[1] == ';') {
            string_discard(line);
#ifndef ONLY_PARSE_TEXTDB
            method = compile(obj, code, &errors);
            list_discard(code);

            /* do warnings and errors, if they exist */
            for (i = 0; i < errors->len; i++)
                frob_n_print_errstr(errors->el[i].u.str->s, name, obj->objnum);

            list_discard(errors);
#endif

            /* return the method, null or not */
            return method;
        }
#ifndef ONLY_PARSE_TEXTDB
        d.u.str = line;
        code = list_add(code, &d);
#endif
        string_discard(line);
    }

    /* We ran out of lines.  This wasn't supposed to happen. */
    DIE("Text dump ended inside method definition!");

    return NULL;
}

/*
// ------------------------------------------------------------------------
*/
#define next_token(__s) { \
        NEXT_SPACE(__s); \
        NEXT_WORD(__s); \
    }

void compile_cdc_file(FILE * fp) {
    Int        new = 0,
               access = A_NONE;
    cStr     * line,
             * str = NULL;
    char     * p,
             * s;
    Obj      * obj;
    off_t      filesize = 0;
    struct stat statbuf;

    fstat(fileno(fp), &statbuf);
    filesize = statbuf.st_size;

    /* start at line 0 */
    line_count = 0;
#ifndef ONLY_PARSE_TEXTDB
    cur_obj = cache_retrieve(ROOT_OBJNUM);
    dump_hash = hash_new(0);
#endif

    /* use fgetstring because it'll expand until we have the whole line */
    while ((line = fgetstring(fp)) && running) {
        line_count++;

        /* Strip trailing spaces from the line. */
        while (line->len && isspace(line->s[line->len - 1]))
            line->len--;
        line->s[line->len] = (char) NULL;

        /* Strip unprintables from the line. */
        for (p = s = line->s; *p; p++) {
            while (*p && !isprint(*p))
                p++;
            *s++ = *p;
        }
        *s = (char) NULL;
        line->len = s - line->s;

        if (!line->len) {
            string_discard(line);
            continue;
        }

        /* if we end in a backslash, concatenate */
        if (line->s[line->len - 1] == '\\') {
            line->s[line->len - 1] = (char) NULL;
            line->len--;
            if (str != NULL) {
                str = string_add(str, line);
                string_discard(line);
            } else {
                str = line;
            }
            continue;
        } else {
            if (str != NULL) {
                str = string_add(str, line);
                string_discard(line);
            } else
                str = line;
        }

        s = str->s;

        /* ignore beginning space */
        NEXT_WORD(s);

        /* old, new or who cares? */
        if (MATCH(s, "new", 3)) {
            new = N_NEW;
            next_token(s);
        } else if (MATCH(s, "old", 3)) {
            new = N_OLD;
            next_token(s);
        } else
            new = N_UNDEF;

        /* access? */
        if (MATCH(s, "public", 6)) {
            access = A_PUBLIC;
            next_token(s);
        } else if (MATCH(s, "protected", 9)) {
            access = A_PROTECTED;
            next_token(s);
        } else if (MATCH(s, "private", 7)) {
            access = A_PRIVATE;
            next_token(s);
        } else if (MATCH(s, "frob", 4)) {
            access = A_FROB;
            next_token(s);
        } else if (MATCH(s, "root", 4)) {
            access = A_ROOT;
            next_token(s);
        } else if (MATCH(s, "driver", 6)) {
            access = A_DRIVER;
            next_token(s);
        } else {
            access = A_NONE;
        }

        if (MATCH(s, "object", 6) || MATCH(s, "as", 2)) {
            if (*s == 'a')
                s += 2;
            else
                s += 6;
            NEXT_WORD(s);
            obj = handle_objcmd(str->s, s, new);
            if (obj != NULL) {
                if (cur_obj != NULL)
                    cache_discard(cur_obj);
                if (print_objs)
                    blank_and_print_obj("Compiling ", (100.0 * ftello(fp)) / filesize, obj);
                cur_obj = obj;
            }
        } else if (MATCH(s, "parent", 6)) {
            s += 6;
            NEXT_WORD(s);
            handle_parcmd(s, new);
        } else if (MATCH(s, "var", 3)) {
            s += 3;
            NEXT_WORD(s);
            handle_varcmd(str->s, s, new, access);
        } else if (MATCH(s, "method", 6)) {
            s += 6;
            NEXT_WORD(s);
            handle_methcmd(fp, s, new, access);
        } else if (MATCH(s, "eval", 4)) {
            s += 4;
            NEXT_WORD(s);
            handle_evalcmd(fp, s, new, access);
        } else if (MATCH(s, "bind_native", 11)) {
            s += 11;
            NEXT_WORD(s);
            handle_bind_nativecmd(fp, s);
        } else if (MATCH(s, "name", 4)) {
            s += 4;
            NEXT_WORD(s);
            handle_namecmd(str->s, s, new);
        } else if (strnccmp(s, "//", 2)) {
            WARN(("parse error, unknown directive."));
            ERRf("\"%s\"\n", s);
            shutdown_coldcc();
        }

        string_discard(str);
        str = NULL;
    }

#ifndef ONLY_PARSE_TEXTDB
    cache_discard(cur_obj);
    verify_native_methods();
#endif

    fputs("\r", stdout);
    fflush(stdout);
#ifndef ONLY_PARSE_TEXTDB
    write_err("Syncing binarydb...");
    cache_sync();
    write_err("Cleaning up name holders...");
    cleanup_holders();
#endif
}

/*
// ------------------------------------------------------------------------
// decompile the binary db to a text file
*/
Int last_length; /* used in doing fancy formatting */
void dump_object(Long objnum, FILE *fp, Bool objnames);
static char * method_definition(Method * m);

#define PRINT_OBJNAME(__obj, __fp) { \
        fputc('$', __fp); \
        fputs(ident_name(__obj->objname), __fp); \
    }
#define PRINT_OBJNUM(__num, __fp) { \
        fprintf(__fp, "#%li", (long) __num); \
    }

static void print_dbref(Obj * obj, cObjnum objnum, FILE * fp, Bool objnames) {
    Bool cachepull = FALSE;

    if (objnames) {
        if (!obj) {
            obj = cache_retrieve(objnum);
            cachepull = TRUE;
        }
        if (!obj || obj->objname == -1)
            PRINT_OBJNUM(objnum, fp)
        else
            PRINT_OBJNAME(obj, fp)
        if (cachepull)
            cache_discard(obj);
    } else {
        PRINT_OBJNUM(objnum, fp);
    }
}

/*
// ------------------------------------------------------------------------
*/
Int text_dump(Bool objnames) {
    FILE      * fp;
    char        buf[BUF];
#ifdef __Win32__
    struct stat statbuf;
#endif

    /* Open the output file. */
    sprintf(buf, "%s.out", c_dir_textdump);

    fp = open_scratch_file(buf, "w");
    if (!fp) {
        write_err("Unable to open temporary file \"%s\".", buf);
        return 0;
    }

    last_length = 0;
    dump_hash = hash_new(0);
    dump_object(ROOT_OBJNUM, fp, objnames);
    hash_discard(dump_hash);

    close_scratch_file(fp);

#ifdef __Win32__
    /* rename() on Win32 won't overwrite a file as it does on Unix */
    if (stat(c_dir_textdump, &statbuf) == 0) {
        unlink(c_dir_textdump);
    }
#endif
    if (rename(buf, c_dir_textdump) == F_FAILURE) {
        write_err("Unable to rename \"%s\" to \"%s\":\n\t%s",
                buf, c_dir_textdump, strerror(GETERR()));
        return 0;
    }

    fputc('\r', stdout);
    fflush(stdout);
    write_err("Done decompiling.");
    return 1;
}
#define is_system(__n) (__n == ROOT_OBJNUM || __n == SYSTEM_OBJNUM)

void dump_object(Long objnum, FILE *fp, Bool objnames) {
    Obj    * obj;
    cList  * objs,
           * code;
    cData  * d,
             dobj;
    cStr   * str;
    Var    * var;
    Int      first,
             i;
    Method * meth;
    static Long objects_decompiled = 0;

    dobj.type = OBJNUM;
    dobj.u.objnum = objnum;

    if (hash_find(dump_hash, &dobj) != F_FAILURE)
        return;

    obj = cache_retrieve(objnum);

    /* try to handle this */
    if (obj == NULL) {
        printf("\rWARNING: NULL object pointer found, you likely used a corrupt binary db!\nWARNING: Attempting to work around.  This will probably create a\nWARNING: textdump with invalid ancestors\n");
        return;
    }

#if 0
    /* have we looked at this object yet? */
    if (obj->search == cur_search) {
        cache_discard(obj);
        return;
    }
#endif

    /* grab the parents list */
    objs = list_dup(obj->parents);
    cache_discard(obj); 

    /* first dump any parents which haven't already been dumped. */
    if (list_length(objs) != 0) {
        for (d = list_first(objs); d; d = list_next(objs, d))
            dump_object(d->u.objnum, fp, objnames);
    }

    if (hash_find(dump_hash, &dobj) != F_FAILURE) {
        list_discard(objs);
        return;
    }
    dump_hash = hash_add(dump_hash, &dobj);

    /* ok, get this object now */
    obj = cache_retrieve(objnum);

#if 0
    /* did we get written out since the last check? */
    if (obj->search == cur_search) {
        list_discard(objs);
        cache_discard(obj);
        return;
    }

    /* ok, lets do it then, mark it dirty and update cur_search */
    obj->dirty = 1;
    obj->search = cur_search;
#endif

    /* let them know? */
    if (print_objs)
        blank_and_print_obj("Decompiling ", (100.0 * ++objects_decompiled) / num_objects, obj);

    /* put 'new' on everything except the system objects */
    if (!is_system(obj->objnum))
       fputs("new ", fp);

    /* print the object definition */
    fputs("object ", fp);
    print_dbref(obj, obj->objnum, fp, objnames);

    /* add the parents */
    if (objs->len != 0) {
        fputc(':', fp);
        fputc(' ', fp);
        first = 1;
        for (d = list_first(objs); d; d = list_next(objs, d)) {
            if (!first)
                fputs(", ", fp);
            first = 0;
            print_dbref(NULL, d->u.objnum, fp, objnames);
        }
    }
    list_discard(objs);
    fputs(";\n", fp);

    /* if we are doing number-only, put a name definition in */
    if (!objnames && obj->objname != -1 && !is_system(obj->objnum)) {
        fputs("name $", fp);
        fputs(ident_name(obj->objname), fp);
        fprintf(fp, " #%li", (long) obj->objnum);
        fputs(";\n", fp);
    }
    fputc('\n', fp);

    /* define variables */
    for (i = 0; i < obj->vars.size; i++) {
        var = &obj->vars.tab[i];
        if (var->name == -1)
            continue;
        if (!cache_check(var->cclass))
            continue;
        str = data_to_literal(&var->val,
                          ((objnames ? DF_WITH_OBJNAMES : 0) | DF_INV_OBJNUMS));
        fputs("var ", fp);
        print_dbref(NULL, var->cclass, fp, objnames);
        fformat(fp, " %I = %S;\n", var->name, str);
        string_discard(str);
    }

    fputc('\n', fp);

    /* define methods */
    if (obj->methods) {
        for (i = 0; i < obj->methods->size; i++) {
            meth = obj->methods->tab[i].m;
            if (!meth)
                continue;
    
            /* define it */
            fputs(method_definition(meth), fp);
    
            /* list it */
            code = decompile(meth, obj, 4, FMT_FULL_PARENS);
            if (list_length(code) == 0) {
                fputs(";\n\n", fp);
            } else {
                fputs(" {\n", fp);
                for (d = list_first(code); d; d = list_next(code, d)) {
                    fputs("    ", fp);
                    fputs(string_chars(d->u.str), fp);
                    putc('\n', fp);
                }
                /* end it */
                fputs("};\n\n", fp);
            }
    
            list_discard(code);
    
            /* if it is native, and they have renamed it, put a rename
               directive down */
            if (meth->m_flags & MF_NATIVE && meth->native != -1) {
                if (strcmp(ident_name(meth->name), natives[meth->native].name))
                    fprintf(fp, "bind_native .%s() .%s();\n\n",
                            natives[meth->native].name,
                            ident_name(meth->name));
            }
        }
        //fputc('\n', fp);
    }
        fputc('\n', fp);

    /* now dump it's children */
    if (obj->children) {
        objs = list_dup(obj->children);
        cache_discard(obj);

        if (objs->len) {
            for (d = list_first(objs); d; d = list_next(objs, d))
                dump_object(d->u.objnum, fp, objnames);
        }
        list_discard(objs);
    } else {
        cache_discard(obj);
    }
}

#define ADD_FLAG(__bit, __str1, __str2) { \
        if (m->m_flags & __bit) { \
            if (flag) \
                strcat(flags, __str1); \
            else { \
                strcpy(flags, __str2); \
                flag++; \
            } \
        } \
    }

static char * method_definition(Method * m) {
    static char   buf[255];
    static char   flags[50];
    char        * s;
    Int           flag = 0;

    /* method access */
    if (m->m_access == MS_PRIVATE)
        strcpy(buf, "private ");
    else if (m->m_access == MS_PROTECTED)
        strcpy(buf, "protected ");
    else if (m->m_access == MS_ROOT)
        strcpy(buf, "root ");
    else if (m->m_access == MS_FROB)
        strcpy(buf, "frob ");
    else if (m->m_access == MS_DRIVER)
        strcpy(buf, "driver ");
    else
        strcpy(buf, "public ");

    /* method name */
    s = ident_name(m->name);

#if 0
    /* this should else and use string_add_unparsed, but, ohwell */
    if (is_valid_ident(s))
#endif

    strcat(buf, "method .");
    strcat(buf, s);
    strcat(buf, "()");

    /* flags */
    if (m->m_flags & MF_NOOVER) {
        strcpy(flags, "nooverride");
        flag++;
    }
    ADD_FLAG(MF_SYNC, ", synchronized", "synchronized");
    ADD_FLAG(MF_LOCK, ", locked", "locked");
    ADD_FLAG(MF_NATIVE, ", native", "native");
    ADD_FLAG(MF_FORK, ", forked", "forked");

    if (flag) {
        strcat(buf, ": ");
        strcat(buf, flags);
    }

    return buf;
}

void blank_and_print_obj(char * what, Float percent_done, Obj * obj) {
    register int x;
    static Int len = 0;
    Number_buf nbuf;
    char * sn;

    /* white out what we just printed */
    if (len) {
        for (x=len; x; x--)
            fputc('\b', stdout);
        for (x=len; x; x--)
            fputc(' ', stdout);
        fputc('\r', stdout);
    }

    /* let them know whats up now */
    len = fprintf(stdout, "%s(%.1f%%) ", what, percent_done);
    if (obj->objname == NOT_AN_IDENT) {
        sn = long_to_ascii(obj->objnum, nbuf);
        fputc('#', stdout);
    } else {  
        sn = ident_name(obj->objname);
        fputc('$', stdout);
    }
    fputs(sn, stdout);
    fputs("...", stdout);

    /* flush */
    fflush(stdout);

    len += strlen(sn) + 4;
}

/* the idea is to do this on strings that may be VERY large */
/* len MUST be more than 4 */
char * strchop(char * str, Int len) {
    register int x;
    for (x=0; x < len; x++) {
        if (str[x] == (char) NULL)
            return str;
    }
    /* null terminate it and put an elipse in */
    str[x] = (char) NULL;
    str[x-1] = str[x-2] = str[x-3] = '.';

    return str;
}

