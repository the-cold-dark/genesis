/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_object_h
#define cdc_object_h

#include "file.h"

struct Obj {
    cList * parents;
    cList * children;

    /* Variables are stored in a table, with index threads starting at the
     * hash table entries.  There is also an index thread for blanks.  This
     * way we can write the hash table to disk easily. */
    struct {
	Var * tab;
	Int * hashtab;
	Int   blanks;
	Int   size;
    } vars;

    /* Methods are also stored in a table.  Since methods are fairly big, we
     * store a table of pointers to methods so that we don't waste lots of
     * space. */
    struct {
	struct mptr {
	    Method * m;
	    Int next;
	}   * tab;
	Int * hashtab;
	Int   blanks;
	Int   size;
    } methods;

    /* Table for string references in methods. */
    String_entry *strings;
    Int num_strings;
    Int strings_size;

    /* Table for identifier references in methods. */
    Ident_entry	*idents;
    Int num_idents;
    Int idents_size;

    /* Information for the cache. */
    cObjnum objnum;
    Int   refs;
    char  dirty;                 /* Flag: Object has been modified. */
    char  dead;	                 /* Flag: Object has been destroyed. */
    Int   ucounter;              /* counter: Object references */

    uLong search;                /* Last cache search to visit this */

    Ident objname;               /* object name */

    /* Pointers to next and previous objects in cache chain. */
    Obj * next;
    Obj * prev;

    /* i/o pointers for faster lookup, only valid in the cache */
    Conn * conn;
    filec_t      * file;
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

struct error_list {
    Int num_errors;
    Int *error_ids;
};

#define START_SEARCH_AT 0 /* zero is the 'unsearched' number */
#define RESET_SEARCH_AT MAX_ULONG
#define SEARCHED(_obj__)      (_obj__->search == cache_search)
#define HAVE_SEARCHED(_obj__) (_obj__->search = cache_search)

#define START_SEARCH() \
    if (cache_search == RESET_SEARCH_AT) \
        cache_search = START_SEARCH_AT; \
    cache_search++
#define END_SEARCH()
#define RETRIEVE_ONCE_OR_RETURN(_obj__, _objnum__) \
    _obj__ = cache_retrieve(_objnum__); \
    if (SEARCHED(_obj__)) { \
        cache_discard(_obj__); \
        return; \
    } \
    HAVE_SEARCHED(_obj__)

/* ..................................................................... */
/* function prototypes */

#ifdef  _object_

/* We use MALLOC_DELTA to keep table sizes to 32 bytes less than a power of
 * two, if pointers and Longs are four bytes. */
/* HACKNOTE: ARRRG, BAD BAD BAD */
#define MALLOC_DELTA            8
#define ANCTEMP_STARTING_SIZE   (32 - MALLOC_DELTA)
#define VAR_STARTING_SIZE       (16 - MALLOC_DELTA - 1)
#define METHOD_STARTING_SIZE    (16 - MALLOC_DELTA - 1)
#define STRING_STARTING_SIZE    (16 - MALLOC_DELTA)
#define IDENTS_STARTING_SIZE    (16 - MALLOC_DELTA)
#define METHOD_CACHE_SIZE       2551

/* ..................................................................... */
/* types and structures */

/* cData for method searches. */
typedef struct search_params Search_params;

struct search_params {
    uLong name;
    Long stop_at;
    Int done;
    Bool is_frob;
    Method * last_method_found;
};

struct {
    Long stamp;
    cObjnum objnum;
    Ident name;
    Bool is_frob;
    cObjnum after;
    cObjnum loc;
} method_cache[METHOD_CACHE_SIZE];

/* ..................................................................... */
/* function prototypes */
static void    object_update_parents(Obj *object,
                                  cList *(*list_op)(cList *, cData *));
static Int     object_has_ancestor_aux(Long objnum, Long ancestor);
static Var    *object_create_var(Obj *object, Long cclass, Long name);
static Var    *object_find_var(Obj *object, Long cclass, Long name);
static Method * method_cache_check(Long objnum, Long name, Long after, Bool is_frob);
static void    method_cache_set(Long objnum, Long name, Long after, Long loc, Bool is_frob);
static void    search_object(Long objnum, Search_params *params);
static void    method_delete_code_refs(Method * method);

Obj *object_new(Long objnum, cList *parents);
void    object_free(Obj *object);
void    object_destroy(Obj *object);
void    object_construct_ancprec(Obj *object);
Int     object_change_parents(Obj *object, cList *parents);
cList * object_ancestors_breadth(Long objnum);
cList * object_ancestors_depth(Long objnum);
cList * object_descendants(Long objnum);
Int     object_has_ancestor(Long objnum, Long ancestor);
void    object_reconstruct_descendent_ancprec(Long objnum);
Int     object_add_string(Obj *object, cStr *string);
void    object_discard_string(Obj *object, Int ind);
cStr *object_get_string(Obj *object, Int ind);
Int     object_add_ident(Obj *object, char *ident);
void    object_discard_ident(Obj *object, Int ind);
Long    object_get_ident(Obj *object, Int ind);
Long    object_add_var(Obj *object, Long name);
Long    object_del_var(Obj *object, Long name);
Long    object_assign_var(Obj *object, Obj *cclass, Long name, cData *val);
Long    object_delete_var(Obj *object, Obj *cclass, Long name);
Long    object_retrieve_var(Obj *object, Obj *cclass, Long name,
                            cData *ret);
Long  object_default_var(Obj *object, Obj *cclass, Long name,
                          cData *ret);
Long  object_inherited_var(Obj *object, Obj *cclass, Long name,
                          cData *ret);
void    object_put_var(Obj *object, Long cclass, Long name, cData *val);
Method * object_find_method(Long objnum, Long name, Bool is_frob);
Method * object_find_method_local(Obj *object, Long name, Bool is_frob);
Method * object_find_next_method(Long objnum, Long name, Long after, Bool is_frob);
Int     object_rename_method(Obj * object, Long oname, Long nname);
void    object_add_method(Obj *object, Long name, Method *method);
Int     object_del_method(Obj *object, Long name);
cList   *object_list_method(Obj *object, Long name, Int indent, int fflags);
Method * method_new(void);
void    method_free(Method *method);
Method *method_dup(Method *method);
void    method_discard(Method *method);
Int     object_set_objname(Obj * object, Long name);
Int     object_del_objname(Obj * object);

/* ..................................................................... */
/* global variables */

/* Count for keeping track of of already-searched objects during searches. */
uLong cache_search;

/* Keeps track of objnum for next object in database. */
Long db_top;

/* Validity count for method cache (incrementing this count invalidates all
 * cache entries. */
static Int cur_stamp = 1;

#else /* _object_ */

extern Obj *object_new(Long objnum, cList *parents);
extern void    object_free(Obj *object);
extern void    object_destroy(Obj *object);
extern void    object_construct_ancprec(Obj *object);
extern Int     object_change_parents(Obj *object, cList *parents);
extern cList  *object_ancestors_breadth(Long objnum);
extern cList  *object_ancestors_depth(Long objnum);
extern cList  *object_descendants(Long objnum);
extern Int     object_has_ancestor(Long objnum, Long ancestor);
extern void    object_reconstruct_descendent_ancprec(Long objnum);
extern Int     object_add_string(Obj *object, cStr *string);
extern void    object_discard_string(Obj *object, Int ind);
extern cStr *object_get_string(Obj *object, Int ind);
extern Int     object_add_ident(Obj *object, char *ident);
extern void    object_discard_ident(Obj *object, Int ind);
extern Long    object_get_ident(Obj *object, Int ind);
extern Long    object_add_var(Obj *object, Long name);
extern Long    object_del_var(Obj *object, Long name);
extern Long    object_assign_var(Obj *object, Obj *cclass, Long name,
                                 cData *val);
extern Long    object_delete_var(Obj *object, Obj *cclass, Long name);
extern Long    object_retrieve_var(Obj *object, Obj *cclass, Long name,
                                   cData *ret);
extern Long    object_default_var(Obj *object, Obj *cclass, Long name,
                          cData *ret);
extern Long    object_inherited_var(Obj *object, Obj *cclass, Long name,
                          cData *ret);
extern void    object_put_var(Obj *object, Long cclass, Long name,
                              cData *val);
extern Method *object_find_method(Long objnum, Long name, Bool is_frob);
extern Method *object_find_method_local(Obj * obj, Long name, Bool is_frob);
extern Method *object_find_next_method(Long objnum, Long name, Long after, Bool is_frob);
extern Int     object_rename_method(Obj * object, Long oname, Long nname);
extern void    object_add_method(Obj *object, Long name, Method *method);
extern Int     object_del_method(Obj *object, Long name);
extern cList   *object_list_method(Obj *object, Long name, Int indent,
                                  int fflags);
extern void    method_free(Method *method);
extern Method *method_dup(Method *method);
extern void    method_discard(Method *method);
extern Int     object_set_objname(Obj * object, Long name);
extern Int     object_del_objname(Obj * object);
extern Int     object_get_method_flags(Obj * object, Long name);
extern Int     object_get_method_access(Obj * object, Long name);
extern Int     object_set_method_flags(Obj * object, Long name, Int flags);
extern Int     object_set_method_access(Obj * object, Long name, Int access);

/* variables */
extern Long db_top;
extern uLong cache_search;

#endif /* _object_ */

#endif /* _object_h_ */
