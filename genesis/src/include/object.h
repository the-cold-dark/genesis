/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_object_h
#define cdc_object_h

#include "defs.h"
#include "file.h"
#include "string_tab.h"

struct _ObjVars {
    Var * tab;
    Int * hashtab;
    Int   blanks;
    Int   size;
};
typedef struct _ObjVars ObjVars;

struct _ObjMethods {
    struct mptr {
        Method * m;
        Int next;
    }   * tab;
    Int * hashtab;
    Int   blanks;
    Int   size;

    /* Table for string references in methods. */
    StringTab *strings;

    /* Table for identifier references in methods. */
    Ident_entry *idents;
    Int num_idents;
    Int idents_size;
};
typedef struct _ObjMethods ObjMethods;

struct Obj {
    /* object identifiers */
    cObjnum     objnum;
    Ident       objname;

    /* object connectivity data */
    cList      *parents;
    cList      *children;
#ifdef USE_PARENT_OBJS
    cList      *parent_objs;
#endif

    /* Variables are stored in a table, with index threads starting at the
     * hash table entries.  There is also an index thread for blanks.  This
     * way we can write the hash table to disk easily. */
    ObjVars     vars;

    /* Methods are also stored in a table.  Since methods are fairly big, we
     * store a table of pointers to methods so that we don't waste lots of
     * space. */
    ObjMethods *methods;

    /* Information for the cache. */
    Int         refs;
    uInt        dirty;                 /* Flag: Object has been modified. */
#ifdef CLEAN_CACHE
    Int         ucounter;              /* counter: Object references */
#endif
    uLong       search;                /* Last cache search to visit this */
    char        dead;                  /* Flag: Object has been destroyed. */

    /* Pointers to next and previous objects in cache chain. */
    Obj        *next_obj;
    Obj        *prev_obj;
#ifdef USE_DIRTY_LIST
    Obj        *next_dirty;
    Obj        *prev_dirty;
#endif

    /* i/o pointers for faster lookup, only valid in the cache */
    Conn       *conn;
    filec_t    *file;
};

/* The object string and identifier tables simplify storage of strings and
 * identifiers for methods.  When we want to free an object without destroying
 * it, we don't need to scan the method code to determine what strings and
 * identifiers to free, and we don't need to do any modification of method
 * code to reflect a new identifier table when we reload the object. */

/* We keep a ref count on object string entries because we have to know when
 * to delete it from the object.  As far as the string is concerned, all these
 * references are really just one reference, since we only duplicate or discard
 * when we're adding or removing a string from an object. */
struct string_entry {
    cStr *str;
    Int refs;
};

/* Similar logic for identifier references. */
struct ident_entry {
    Ident id;
    Int refs;
};

struct var {
    Ident name;
    cObjnum cclass;
    cData val;
    Int next;
};

struct error_list {
    Int num_errors;
    Int *error_ids;
};

struct Method {
    Ident name;
    Obj *object;
    Int num_args;
    Object_ident *argnames;
    Object_ident rest;
    Int num_vars;
    Object_ident *varnames;
    Int num_opcodes;
    Long *opcodes;
    Int num_error_lists;
    Error_list *error_lists;

    /* if this is a native method, it is > 0 and is relative to the native
       method's spot in the lookup table */
    Int native;

    /* consolidate the following into bit flags */
    Int m_access;       /* public, protected, private */
    Int m_flags;       /* overridable, synchronized, locked */
    Int refs;
};

/* access: only one at a time */
#define MS_PUBLIC    1    /* public */
#define MS_PROTECTED 2    /* protected */
#define MS_PRIVATE   4    /* private */
#define MS_ROOT      8    /* root */
#define MS_DRIVER    16   /* sender() and caller() are 0 */
#define MS_FROB      32   /* called from a frob: (<$frob, #[]>).method() */

/* perhaps create a method reference to call a task on for perm
   checking, with sender() and caller()? */

/* flags: any number of the following */
#define MF_NONE      0    /* No flags */
#define MF_NOOVER    1    /* not overridable */
#define MF_SYNC      2    /* synchronized */
#define MF_LOCK      4    /* locked */
#define MF_NATIVE    8    /* native */
#define MF_FORK      16   /* fork */
#define MF_UNDF2     32   /* undefined */
#define MF_UNDF3     64   /* undefined */
#define MF_UNDF4     128  /* undefined */

#define FROB_YES 1
#define FROB_NO  0
#define FROB_ANY 2
#define FROB_RETRY -1

/* Needed here for defs.c and cache.c */
#define START_SEARCH_AT 0 /* zero is the 'unsearched' number */

/* ..................................................................... */
/* function prototypes */

extern Obj    *object_new(cObjnum objnum, cList *parents);
extern void    object_alloc_methods(Obj *object);
extern void    object_free(Obj *object);
extern void    object_destroy(Obj *object);
extern void    object_construct_ancprec(Obj *object);
extern Int     object_change_parents(Obj *object, cList *parents);
extern cList  *object_ancestors_breadth(cObjnum objnum);
extern cList  *object_ancestors_depth(cObjnum objnum);
extern cList  *object_descendants(cObjnum objnum);
extern Int     object_has_ancestor(cObjnum objnum, cObjnum ancestor);
extern void    object_reconstruct_descendent_ancprec(cObjnum objnum);
extern Int     object_add_string(Obj *object, cStr *string);
extern void    object_discard_string(Obj *object, Int ind);
extern cStr   *object_get_string(Obj *object, Int ind);
extern Int     object_add_ident(Obj *object, char *ident);
extern void    object_discard_ident(Obj *object, Int ind);
extern Ident   object_get_ident(Obj *object, Int ind);
extern Bool    object_defines_var(cObjnum object, Ident name);
extern Ident   object_add_var(Obj *object, Ident name);
extern Ident   object_del_var(Obj *object, Ident name);
extern Ident   object_assign_var(Obj *object, Obj *cclass, Ident name,
                                 cData *val);
extern Ident   object_delete_var(Obj *object, Obj *cclass, Ident name);
extern Ident   object_retrieve_var(Obj *object, Obj *cclass, Ident name,
                                   cData *ret);
extern Ident   object_default_var(Obj *object, Obj *cclass, Ident name,
                                  cData *ret);
extern Ident   object_inherited_var(Obj *object, Obj *cclass, Ident name,
                                    cData *ret);
extern Bool    object_put_var(Obj *object, cObjnum cclass, Ident name,
                              cData *val);
extern Method *object_find_method(cObjnum objnum, Ident name, Bool is_frob);
extern Method *object_find_method_local(Obj * obj, Ident name, Bool is_frob);
extern Method *object_find_next_method(cObjnum objnum, Ident name,
                                       cObjnum after, Bool is_frob);
extern Int     object_rename_method(Obj * object, Ident oname, Ident nname);
extern void    object_add_method(Obj *object, Ident name, Method *method);
extern Int     object_del_method(Obj *object, Ident name, Bool replacing);
extern cList  *object_list_method(Obj *object, Ident name, Int indent,
                                  int fflags);
extern Method *method_new(void);
extern void    method_free(Method *method);
extern Method *method_dup(Method *method);
extern void    method_discard(Method *method);
extern Int     object_set_objname(Obj * object, Ident name);
extern Int     object_del_objname(Obj * object);
extern Int     object_get_method_flags(Obj * object, Ident name);
extern Int     object_get_method_access(Obj * object, Ident name);
extern Int     object_set_method_flags(Obj * object, Ident name, Int flags);
extern Int     object_set_method_access(Obj * object, Ident name, Int access);
extern Bool    object_has_methods(Obj *object);
#ifdef USE_PARENT_OBJS
extern void    object_load_parent_objs(Obj * obj);
#endif

extern cList  *ancestor_cache_info(void);
extern cList  *method_cache_info(void);

/* variables */
extern Long    db_top;
extern Long    num_objects;
extern uLong   cache_search;

#endif /* _object_h_ */

