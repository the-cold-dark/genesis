/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/object.h
// ---
// Declarations for objects.
//
// The header ordering conventions break down here; we need to make sure data.h
// has finished, not just that the typedefs have been done.
*/

#ifndef _object_h_
#define _object_h_

#include <stdio.h>
#include "cdc_types.h"
#include "io.h"
#include "file.h"

struct object {
    list_t * parents;
    list_t * children;

    /* Variables are stored in a table, with index threads starting at the
     * hash table entries.  There is also an index thread for blanks.  This
     * way we can write the hash table to disk easily. */
    struct {
	Var * tab;
	int * hashtab;
	int   blanks;
	int   size;
    } vars;

    /* Methods are also stored in a table.  Since methods are fairly big, we
     * store a table of pointers to methods so that we don't waste lots of
     * space. */
    struct {
	struct mptr {
	    method_t * m;
	    int next;
	}   * tab;
	int * hashtab;
	int   blanks;
	int   size;
    } methods;

    /* Table for string references in methods. */
    String_entry *strings;
    int num_strings;
    int strings_size;

    /* Table for identifier references in methods. */
    Ident_entry	*idents;
    int num_idents;
    int idents_size;

    /* Information for the cache. */
    objnum_t objnum;
    int   refs;
    char  dirty;                 /* Flag: Object has been modified. */
    char  dead;	                 /* Flag: Object has been destroyed. */
    int   ucounter;              /* counter: Object references */

    long search;                 /* Last search to visit object. */

    Ident objname;               /* object name */

    /* Pointers to next and previous objects in cache chain. */
    object_t * next;
    object_t * prev;

    /* i/o pointers for faster lookup, only valid in the cache */
    connection_t * conn;
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
    string_t *str;
    int refs;
};

/* Similar logic for identifier references. */
struct ident_entry {
    Ident id;
    int refs;
};

struct var {
    Ident name;
    objnum_t cclass;
    data_t val;
    int next;
};

struct method {
    Ident name;
    object_t *object;
    int num_args;
    Object_ident *argnames;
    Object_ident rest;
    int num_vars;
    Object_ident *varnames;
    int num_opcodes;
    long *opcodes;
    int num_error_lists;
    Error_list *error_lists;

    /* consolidate the following into bit flags */
    int m_access;       /* public, protected, private */
    int m_flags;       /* overridable, synchronized, locked */
    int refs;
};

/* access: only one at a time */
#define MS_PUBLIC    0x1    /* public */
#define MS_PROTECTED 0x2    /* protected */
#define MS_PRIVATE   0x4    /* private */
#define MS_ROOT      0x8    /* root */
#define MS_DRIVER    0x16   /* sender() and caller() are 0 */

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

struct error_list {
    int num_errors;
    int *error_ids;
};

/* ..................................................................... */
/* function prototypes */

#ifdef  _object_


/* We use MALLOC_DELTA to keep table sizes to 32 bytes less than a power of
 * two, if pointers and longs are four bytes. */
#define MALLOC_DELTA            8
#define ANCTEMP_STARTING_SIZE   (32 - MALLOC_DELTA)
#define VAR_STARTING_SIZE       (16 - MALLOC_DELTA - 1)
#define METHOD_STARTING_SIZE    (16 - MALLOC_DELTA - 1)
#define STRING_STARTING_SIZE    (16 - MALLOC_DELTA)
#define IDENTS_STARTING_SIZE    (16 - MALLOC_DELTA)
#define METHOD_CACHE_SIZE       503

/* ..................................................................... */
/* types and structures */

/* data_t for method searches. */
typedef struct search_params Search_params;

struct search_params {
    unsigned long name;
    long stop_at;
    int done;
    method_t * last_method_found;
};

struct {
    long stamp;
    objnum_t objnum;
    Ident name;
    objnum_t after;
    objnum_t loc;
} method_cache[METHOD_CACHE_SIZE];

/* ..................................................................... */
/* function prototypes */
static void    object_update_parents(object_t *object,
                                  list_t *(*list_op)(list_t *, data_t *));
static list_t   *object_descendants_aux(long objnum, list_t *descendants);
static list_t   *object_ancestors_aux(long objnum, list_t *ancestors);
static int     object_has_ancestor_aux(long objnum, long ancestor);
static Var    *object_create_var(object_t *object, long cclass, long name);
static Var    *object_find_var(object_t *object, long cclass, long name);
static method_t * object_find_method_local(object_t *object, long name);
static method_t * method_cache_check(long objnum, long name, long after);
static void    method_cache_set(long objnum, long name, long after, long loc);
static void    search_object(long objnum, Search_params *params);
static void    method_delete_code_refs(method_t * method);

object_t *object_new(long objnum, list_t *parents);
void    object_free(object_t *object);
void    object_destroy(object_t *object);
void    object_construct_ancprec(object_t *object);
int     object_change_parents(object_t *object, list_t *parents);
list_t *object_ancestors(long objnum);
list_t *object_descendants(long objnum);
int     object_has_ancestor(long objnum, long ancestor);
void    object_reconstruct_descendent_ancprec(long objnum);
int     object_add_string(object_t *object, string_t *string);
void    object_discard_string(object_t *object, int ind);
string_t *object_get_string(object_t *object, int ind);
int     object_add_ident(object_t *object, char *ident);
void    object_discard_ident(object_t *object, int ind);
long    object_get_ident(object_t *object, int ind);
long    object_add_var(object_t *object, long name);
long    object_del_var(object_t *object, long name);
long    object_assign_var(object_t *object, object_t *cclass, long name, data_t *val);
long    object_delete_var(object_t *object, object_t *cclass, long name);
long    object_retrieve_var(object_t *object, object_t *cclass, long name,
                            data_t *ret);
void    object_put_var(object_t *object, long cclass, long name, data_t *val);
method_t * object_find_method(long objnum, long name);
method_t * object_find_next_method(long objnum, long name, long after);
int     object_rename_method(object_t * object, long oname, long nname);
void    object_add_method(object_t *object, long name, method_t *method);
int     object_del_method(object_t *object, long name);
list_t   *object_list_method(object_t *object, long name, int indent, int parens);
void    method_free(method_t *method);
method_t *method_dup(method_t *method);
void    method_discard(method_t *method);
int     object_set_objname(object_t * object, long name);
int     object_del_objname(object_t * object);

/* ..................................................................... */
/* global variables */

/* Count for keeping track of of already-searched objects during searches. */
long cur_search;

/* Keeps track of objnum for next object in database. */
long db_top;

/* Validity count for method cache (incrementing this count invalidates all
 * cache entries. */
static int cur_stamp = 1;

#else /* _object_ */

extern object_t *object_new(long objnum, list_t *parents);
extern void    object_free(object_t *object);
extern void    object_destroy(object_t *object);
extern void    object_construct_ancprec(object_t *object);
extern int     object_change_parents(object_t *object, list_t *parents);
extern list_t   *object_ancestors(long objnum);
extern list_t   *object_descendants(long objnum);
extern int     object_has_ancestor(long objnum, long ancestor);
extern void    object_reconstruct_descendent_ancprec(long objnum);
extern int     object_add_string(object_t *object, string_t *string);
extern void    object_discard_string(object_t *object, int ind);
extern string_t *object_get_string(object_t *object, int ind);
extern int     object_add_ident(object_t *object, char *ident);
extern void    object_discard_ident(object_t *object, int ind);
extern long    object_get_ident(object_t *object, int ind);
extern long    object_add_var(object_t *object, long name);
extern long    object_del_var(object_t *object, long name);
extern long    object_assign_var(object_t *object, object_t *cclass, long name,
                                 data_t *val);
extern long    object_delete_var(object_t *object, object_t *cclass, long name);
extern long    object_retrieve_var(object_t *object, object_t *cclass, long name,
                                   data_t *ret);
extern void    object_put_var(object_t *object, long cclass, long name,
                              data_t *val);
extern method_t *object_find_method(long objnum, long name);
extern method_t *object_find_next_method(long objnum, long name, long after);
extern int     object_rename_method(object_t * object, long oname, long nname);
extern void    object_add_method(object_t *object, long name, method_t *method);
extern int     object_del_method(object_t *object, long name);
extern list_t   *object_list_method(object_t *object, long name, int indent,
                                  int parens);
extern void    method_free(method_t *method);
extern method_t *method_dup(method_t *method);
extern void    method_discard(method_t *method);
extern int     object_set_objname(object_t * object, long name);
extern int     object_del_objname(object_t * object);
extern int     object_get_method_flags(object_t * object, long name);
extern int     object_get_method_access(object_t * object, long name);
extern int     object_set_method_flags(object_t * object, long name, int flags);
extern int     object_set_method_access(object_t * object, long name, int access);

/* variables */
extern long db_top;
extern long cur_search;

#endif /* _object_ */

#endif /* _object_h_ */
