/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: native.c
// ---
//
// this file handles native methods
//
// adding native methods this way is inefficient, but we only do it at bootup,
// and it guarantee's that the array is only as big as we need it, as it does
// not grow
*/

#define _modules_

#include "config.h"
#include "defs.h"
#include "object.h"
#include "native.h"
#include "memory.h"

#if 0
typedef struct native_s {
    char     * name;
    char     * bindobj;
    void (*func)(void);
    int        args;
    int        rest;
} native_t;
#endif

int n_top;

void init_natives(void) {
    natives = (native_t **) malloc(sizeof(native_t *));
}

int init_native_methods(void) {
    native_methods = EMALLOC(native_t, 1);
    num_natives = 0;
    native_methods[0] = NULL;
}

int add_native_method(native_t * native) {
    Ident id;
    object_t * obj;
    method_t * method = NULL;
    objnum_t   objnum;

    /* twit */
    if (strlen(native->bindobj) < 2 || *native->name == (char) NULL)
        return 0;

    /* find the object */
    if (*native->bindobj == '#')
        objnum = atol(++native->bindobj);
    else {
        if (*native->bindobj == '$')
            native->bindobj++;

        id = ident_get(native->bindobj);
        lookup_retrieve_name(id, &objnum);
        ident_discard(id);
    }

    if (objnum == INV_OBJNUM || !cache_check(objnum))
        return 0;

    /* get the name for the method, if the object already has a method
       by that name, ignore the native method; but let somebody know */
    id = ident_get(native->name);
    method = object_find_method(objnum, id);
    if (method != NULL) {
        ident_discard(id);
        return 0;
    }

    method = method_new();
    method->num_args = native->args;
    method->rest = (native->rest == 0) ? 0 : -1;
    method->refs = 1; /* we are referencing it now */

    /* everything else is never used, so initialize them to NULL/zero values */
    method->argnames = (Object_ident *) NULL;
    method->varnames = (Object_ident *) NULL;
    method->error_lists = (Error_list *) NULL;
    method->opcodes = (long *) NULL;
    method->num_vars = 0;
    method->num_opcodes = 0;
    method->num_error_lists = 0;

    /* allocate a new spot for it in the list of pointers */
    n_top++;
    natives = (native_t **)
        erealloc(natives, (n_top + 1) * sizeof(native_t *));
    natives[n_top] = native;

    /* set the native pointer in the method */
    method->native = n_top;

    /* add it to the object */
    obj = cache_retrieve(objnum);
    object_add_method(obj, id, method);
}

#undef _modules_
