/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: object.c
// ---
// Object manipulation routines.
*/

#define _object_

#include "config.h"
#include "defs.h"

#include <string.h>
#include "y.tab.h"
#include "object.h"
#include "cdc_types.h"
#include "data.h"
#include "memory.h"
#include "opcodes.h"
#include "cache.h"
#include "io.h"
#include "ident.h"
#include "cdc_string.h"
#include "decode.h"
#include "util.h"
#include "log.h"
#include "lookup.h"

/*
// -----------------------------------------------------------------
//
// Error-checking on parents is the job of the calling function.  Also,
// dire things may happen if an object numbered objnum already exists.
//
*/

object_t * object_new(long objnum, list_t * parents) {
    object_t * cnew;
    int        i;

    if (objnum == -1)
	objnum = db_top++;
    else if (objnum >= db_top)
	db_top = objnum + 1;

    cnew = cache_get_holder(objnum);
    cnew->parents = list_dup(parents);
    cnew->children = list_new(0);

    /* Initialize variables table and hash table. */
    cnew->vars.tab = EMALLOC(Var, VAR_STARTING_SIZE);
    cnew->vars.hashtab = EMALLOC(int, VAR_STARTING_SIZE);
    cnew->vars.blanks = 0;
    cnew->vars.size = VAR_STARTING_SIZE;
    for (i = 0; i < VAR_STARTING_SIZE; i++) {
	cnew->vars.hashtab[i] = -1;
	cnew->vars.tab[i].name = -1;
	cnew->vars.tab[i].next = i + 1;
    }
    cnew->vars.tab[VAR_STARTING_SIZE - 1].next = -1;

    /* Initialize methods table and hash table. */
    cnew->methods.tab = EMALLOC(struct mptr, METHOD_STARTING_SIZE);
    cnew->methods.hashtab = EMALLOC(int, METHOD_STARTING_SIZE);
    cnew->methods.blanks = 0;
    cnew->methods.size = METHOD_STARTING_SIZE;
    for (i = 0; i < METHOD_STARTING_SIZE; i++) {
	cnew->methods.hashtab[i] = -1;
	cnew->methods.tab[i].m = NULL;
	cnew->methods.tab[i].next = i + 1;
    }
    cnew->methods.tab[METHOD_STARTING_SIZE - 1].next = -1;

    /* Initialize string table. */
    cnew->strings = EMALLOC(String_entry, STRING_STARTING_SIZE);
    cnew->strings_size = STRING_STARTING_SIZE;
    cnew->num_strings = 0;

    /* Initialize identifier table. */
    cnew->idents = EMALLOC(Ident_entry, IDENTS_STARTING_SIZE);
    cnew->idents_size = IDENTS_STARTING_SIZE;
    cnew->num_idents = 0;

    cnew->search = 0;

    /* Add this object to the children list of parents. */
    object_update_parents(cnew, list_add);

    cnew->objname = -1;
    cnew->conn = NULL;
    cnew->file = NULL;

    cnew->dirty = 1;

    return cnew;
}

/*
// -----------------------------------------------------------------
//
// Free the data on an object, as when we're swapping it out.  Since the object
// probably still exists on disk, we don't free parent and child references;
// also, we don't free code references on methods because that would make
// swapping too difficult.
//
*/
void object_free(object_t *object) {
    int i;

    /* Free parents and children list. */
    list_discard(object->parents);
    list_discard(object->children);

    /* Free variable names and contents. */
    for (i = 0; i < object->vars.size; i++) {
	if (object->vars.tab[i].name != -1) {
	    ident_discard(object->vars.tab[i].name);
	    data_discard(&object->vars.tab[i].val);
	}
    }
    efree(object->vars.tab);
    efree(object->vars.hashtab);

    /* Free methods. */
    for (i = 0; i < object->methods.size; i++) {
	if (object->methods.tab[i].m)
	    method_free(object->methods.tab[i].m);
    }
    efree(object->methods.tab);
    efree(object->methods.hashtab);

    /* Discard strings. */
    for (i = 0; i < object->num_strings; i++) {
	if (object->strings[i].str)
	    string_discard(object->strings[i].str);
    }
    efree(object->strings);

    /* Discard identifiers. */
    for (i = 0; i < object->num_idents; i++) {
	if (object->idents[i].id != NOT_AN_IDENT) {
	  ident_discard(object->idents[i].id);
	}
    }
    efree(object->idents);
}

/*
// -----------------------------------------------------------------
//
// Free everything on the object, update parents and descendents, etc.  The
// object is really going to be gone.  We don't want anything left, except for
// the structure it came in, which belongs to the cache.
//
*/

void object_destroy(object_t *object) {
    list_t *children;
    data_t *d, cthis;
    object_t *kid;

    /* Invalidate the method cache. */
    cur_stamp++;

    /* remove the object name, if it has one */
    object_del_objname(object);

    /* Tell parents we don't exist any more. */
    object_update_parents(object, list_delete_element);

    /* Tell children the same thing (no function for this, just do it).
     * Also, check if any kid hits zero parents, and reparent it to our
     * parents if it does. */
    children = object->children;

    cthis.type = OBJNUM;
    cthis.u.objnum = object->objnum;
    for (d = list_first(children); d; d = list_next(children, d)) {
	kid = cache_retrieve(d->u.objnum);
	kid->parents = list_delete_element(kid->parents, &cthis);
	if (!kid->parents->len) {
	    list_discard(kid->parents);
	    kid->parents = list_dup(object->parents);
	    object_update_parents(kid, list_add);
	}
	kid->dirty = 1;
	cache_discard(kid);
    }

    /* boot the connection on this object (if it exists) */
    boot(object);

    /* close the file on this object (if there is one) */
    abort_file(object->file);

    /* Having freed all the stuff we don't normally free, free the stuff that
     * we do normally free. */
    object_free(object);
}

/*
// -----------------------------------------------------------------
*/
INTERNAL void object_update_parents(object_t * object,
				    list_t *(*list_op)(list_t *, data_t *))
{
    object_t * p;
    list_t     * parents;
    data_t     * d,
               cthis;

    /* Make a data structure for the children list. */
    cthis.type = OBJNUM;
    cthis.u.objnum = object->objnum;

    parents = object->parents;

    for (d = list_first(parents); d; d = list_next(parents, d)) {
	p = cache_retrieve(d->u.objnum);
	p->children = (*list_op)(p->children, &cthis);
	p->dirty = 1;
	cache_discard(p);
    }
}

/*
// -----------------------------------------------------------------
//
// Note: descendants is something which should NOT be used often, it is
// for maintenance purposes.
//
*/
list_t * object_descendants(long objnum) {
    cur_search++;
    return object_descendants_aux(objnum, list_new(0));
}

INTERNAL list_t * object_descendants_aux(long objnum, list_t * list) {
    object_t * obj;
    list_t   * children;
    data_t   * d,
               cthis;

    obj = cache_retrieve(objnum);
    if (obj->search == cur_search) {
	cache_discard(obj);
	return list;
    }
    obj->dirty = 1;
    obj->search = cur_search;

    children = list_dup(obj->children);

    cache_discard(obj);

    for (d = list_first(children); d; d = list_next(children, d))
	list = object_descendants_aux(d->u.objnum, list);

    list_discard(children);

    cthis.type = OBJNUM;
    cthis.u.objnum = objnum;
    return list_add(list, &cthis);
}

/*
// -----------------------------------------------------------------
*/
list_t * object_ancestors(long objnum) {
    list_t * ancestors;

    /* Get the ancestor list, backwards. */
    ancestors = list_new(0);
    cur_search++;
    ancestors = object_ancestors_aux(objnum, ancestors);

    return list_reverse(ancestors);
}

/* Modifies ancestors.  Returns a backwards list. */
INTERNAL list_t *object_ancestors_aux(long objnum, list_t *ancestors) {
    object_t *object;
    list_t *parents;
    data_t *d, cthis;

    object = cache_retrieve(objnum);
    if (object->search == cur_search) {
	cache_discard(object);
	return ancestors;
    }
    object->dirty = 1;
    object->search = cur_search;

    parents = list_dup(object->parents);
    cache_discard(object);

    for (d = list_last(parents); d; d = list_prev(parents, d))
	ancestors = object_ancestors_aux(d->u.objnum, ancestors);
    list_discard(parents);

    cthis.type = OBJNUM;
    cthis.u.objnum = objnum;
    return list_add(ancestors, &cthis);
}

int object_has_ancestor(long objnum, long ancestor)
{
    if (objnum == ancestor)
	return 1;
    cur_search++;
    return object_has_ancestor_aux(objnum, ancestor);
}

static int object_has_ancestor_aux(long objnum, long ancestor)
{
    object_t *object;
    list_t *parents;
    data_t *d;

    object = cache_retrieve(objnum);

    /* Don't search an object twice. */
    if (object->search == cur_search) {
	cache_discard(object);
	return 0;
    }
    object->dirty = 1;
    object->search = cur_search;

    parents = list_dup(object->parents);
    cache_discard(object);

    for (d = list_first(parents); d; d = list_next(parents, d)) {
	if (d->u.objnum == ancestor) {
	    list_discard(parents);
	    return 1;
	}
    }

    for (d = list_first(parents); d; d = list_next(parents, d)) {
	if (object_has_ancestor_aux(d->u.objnum, ancestor)) {
	    list_discard(parents);
	    return 1;
	}
    }

    list_discard(parents);
    return 0;
}

int object_change_parents(object_t *object, list_t *parents)
{
    objnum_t parent;
    data_t *d;

    /* Make sure that all parents are valid objects, and that they don't create
     * any cycles.  If something is wrong, return the index of the parent that
     * caused the problem. */
    for (d = list_first(parents); d; d = list_next(parents, d)) {
	if (d->type != OBJNUM)
	    return d - list_first(parents);
	parent = d->u.objnum;
	if (!cache_check(parent) || object_has_ancestor(parent, object->objnum))
	    return d - list_first(parents);
    }

    /* Invalidate the method cache. */
    cur_stamp++;

    object->dirty = 1;

    /* Tell our old parents that we're no longer a kid, and discard the old
     * parents list. */
    object_update_parents(object, list_delete_element);
    list_discard(object->parents);

    /* Set the object's parents list to a copy of the new list, and tell all
     * our new parents that we're a kid. */
    object->parents = list_dup(parents);
    object_update_parents(object, list_add);

    /* Return -1, meaning that all the parents were okay. */
    return -1;
}

int object_add_string(object_t *object, string_t *str)
{
    int i, blank = -1;

    /* Get the object dirty now, so we can return with a clean conscience. */
    object->dirty = 1;

    /* Look for blanks while checking for an equivalent string. */
    for (i = 0; i < object->num_strings; i++) {
	if (!object->strings[i].str) {
	    blank = i;
	} else if (string_cmp(str, object->strings[i].str) == 0) {
	    object->strings[i].refs++;
	    return i;
	}
    }

    /* Fill in a blank if we found one. */
    if (blank != -1) {
	object->strings[blank].str = string_dup(str);
	object->strings[blank].refs = 1;
	return blank;
    }

    /* Check to see if we have to enlarge the table. */
    if (i >= object->strings_size) {
	object->strings_size = object->strings_size * 2 + MALLOC_DELTA;
	object->strings = EREALLOC(object->strings, String_entry,
				   object->strings_size);
    }

    /* Add the string to the end of the table. */
    object->strings[i].str = string_dup(str);
    object->strings[i].refs = 1;
    object->num_strings++;

    return i;
}

void object_discard_string(object_t *object, int ind)
{
    object->strings[ind].refs--;
    if (!object->strings[ind].refs) {
	string_discard(object->strings[ind].str);
	object->strings[ind].str = NULL;
    }

    object->dirty = 1;
}

string_t *object_get_string(object_t *object, int ind)
{
    return object->strings[ind].str;
}

int object_add_ident(object_t *object, char *ident)
{
    int i, blank = -1;
    long id;

    /* Mark the object dirty, since we will modify it in all cases. */
    object->dirty = 1;

    /* Get an identifier for the identifier string. */
    id = ident_get(ident);

    /* Look for blanks while checking for an equivalent identifier. */
    for (i = 0; i < object->num_idents; i++) {
	if (object->idents[i].id == -1) {
	    blank = i;
	} else if (object->idents[i].id == id) {
	    /* We already have this id.  Up the reference count on the object's
	     * copy if it, discard this function's copy of it, and return the
	     * index into the object's identifier table. */
	  object->idents[i].refs++;
	  ident_discard(id);
	  return i;
	}
    }

    /* Fill in a blank if we found one. */
    if (blank != -1) {
	object->idents[blank].id = id;
	object->idents[blank].refs = 1;
	return blank;
    }

    /* Check to see if we have to enlarge the table. */
    if (i >= object->idents_size) {
	object->idents_size = object->idents_size * 2 + MALLOC_DELTA;
	object->idents = EREALLOC(object->idents, Ident_entry,
				  object->idents_size);
    }

    /* Add the string to the end of the table. */
    object->idents[i].id = id;
    object->idents[i].refs = 1;
    object->num_idents++;

    return i;
}

void object_discard_ident(object_t *object, int ind)
{
    object->idents[ind].refs--;
    if (!object->idents[ind].refs) {
      /*write_err("##object_discard_ident %d %s",
	object->idents[ind].id, ident_name(object->idents[ind].id));*/

      ident_discard(object->idents[ind].id);
      object->idents[ind].id = NOT_AN_IDENT;
    }

    object->dirty = 1;
}

long object_get_ident(object_t *object, int ind) {
    return object->idents[ind].id;
}

long object_add_var(object_t *object, long name) {
    if (object_find_var(object, object->objnum, name))
	return varexists_id;
    object_create_var(object, object->objnum, name);
    return NOT_AN_IDENT;
}

long object_del_var(object_t *object, long name)
{
    int *indp;
    Var *var;

    /* This is the index-thread equivalent of double pointers in a standard
     * linked list.  We traverse the list using pointers to the ->next element
     * of the variables. */
    indp = &object->vars.hashtab[hash(ident_name(name)) % object->vars.size];
    for (; *indp != -1; indp = &object->vars.tab[*indp].next) {
	var = &object->vars.tab[*indp];
	if (var->name == name && var->cclass == object->objnum) {
	/*  write_err("##object_del_var %d %s", var->name, ident_name(var->name));*/
	    ident_discard(var->name);
	    data_discard(&var->val);
	    var->name = -1;

	    /* Remove ind from hash table thread, and add it to blanks
	     * thread. */
	    *indp = var->next;
	    var->next = object->vars.blanks;
	    object->vars.blanks = var - object->vars.tab;

	    object->dirty = 1;
	    return NOT_AN_IDENT;
	}
    }

    return varnf_id;
}

long object_assign_var(object_t *object, object_t *cclass, long name, data_t *val) {
    Var *var;

    /* Make sure variable exists in cclass (method object). */
    if (!object_find_var(cclass, cclass->objnum, name))
	return varnf_id;

    /* Get variable slot on object, creating it if necessary. */
    var = object_find_var(object, cclass->objnum, name);
    if (!var)
	var = object_create_var(object, cclass->objnum, name);

    data_discard(&var->val);
    data_dup(&var->val, val);

    object->dirty = 1;

    return NOT_AN_IDENT;
}

long object_delete_var(object_t *object, object_t *cclass, long name) {
    Var *var;
    int *indp;

    /* find the parameter definition in cclass (method object). */
    if (!object_find_var(cclass, cclass->objnum, name))
        return varnf_id;

    /* Get variable slot on object */
    var = object_find_var(object, cclass->objnum, name);
    if (var) {
        indp=&object->vars.hashtab[hash(ident_name(name))%object->vars.size];
        for (; *indp != -1; indp = &object->vars.tab[*indp].next) {
            var = &object->vars.tab[*indp];
            if (var->name == name && var->cclass == cclass->objnum) {
                ident_discard(var->name);
                data_discard(&var->val);
                var->name = -1;

                /* Remove ind from hash table thread, and add it to blanks
                 * thread. */
                *indp = var->next;
                var->next = object->vars.blanks;
                object->vars.blanks = var - object->vars.tab;

                object->dirty = 1;
                return NOT_AN_IDENT;
            }
        }
    }

    return varnf_id;
}

long object_retrieve_var(object_t *object, object_t *cclass, long name, data_t *ret)
{
    Var *var;

    /* Make sure variable exists on cclass. */
    if (!object_find_var(cclass, cclass->objnum, name))
	return varnf_id;

    var = object_find_var(object, cclass->objnum, name);
    if (var) {
	data_dup(ret, &var->val);
    } else {
	ret->type = INTEGER;
	ret->u.val = 0;
    }

    return NOT_AN_IDENT;
}

/* Only the text dump reader calls this function; it assigns or creates a
 * variable as needed, and always succeeds. */
void object_put_var(object_t *object, long cclass, long name, data_t *val)
{
    Var *var;

    var = object_find_var(object, cclass, name);
    if (!var)
	var = object_create_var(object, cclass, name);
    data_discard(&var->val);
    data_dup(&var->val, val);
}

/* Add a variable to an object. */
static Var *object_create_var(object_t *object, long cclass, long name)
{
    Var *cnew;
    int ind;

    /* If the variable table is full, expand it and its corresponding hash
     * table. */
    if (object->vars.blanks == -1) {
	int new_size, i;

	/* Compute new size and resize tables. */
	new_size = object->vars.size * 2 + MALLOC_DELTA + 1;
	object->vars.tab = EREALLOC(object->vars.tab, Var, new_size);
	object->vars.hashtab = EREALLOC(object->vars.hashtab, int, new_size);

	/* Refill hash table. */
	for (i = 0; i < new_size; i++)
	    object->vars.hashtab[i] = -1;
	for (i = 0; i < object->vars.size; i++) {
	    ind = hash(ident_name(object->vars.tab[i].name)) % new_size;
	    object->vars.tab[i].next = object->vars.hashtab[ind];
	    object->vars.hashtab[ind] = i;
	}

	/* Create new thread of blanks, setting names to -1. */
	for (i = object->vars.size; i < new_size; i++) {
	    object->vars.tab[i].name = -1;
	    object->vars.tab[i].next = i + 1;
	}
	object->vars.tab[new_size - 1].next = -1;
	object->vars.blanks = object->vars.size;

	object->vars.size = new_size;
    }

    /* Add variable at first blank. */
    cnew = &object->vars.tab[object->vars.blanks];
    object->vars.blanks = cnew->next;

    /* Fill in new variable. */
    cnew->name = ident_dup(name);
    cnew->cclass = cclass;
    cnew->val.type = INTEGER;
    cnew->val.u.val = 0;

    /* Add variable to hash table thread. */
    ind = hash(ident_name(name)) % object->vars.size;
    cnew->next = object->vars.hashtab[ind];
    object->vars.hashtab[ind] = cnew - object->vars.tab;

    object->dirty = 1;

    return cnew;
}

/* Look for a variable on an object. */
static Var *object_find_var(object_t *object, long cclass, long name)
{
    int ind;
    Var *var;

    /* Traverse hash table thread, stopping if we get a match on the name. */
    ind = object->vars.hashtab[hash(ident_name(name)) % object->vars.size];
    for (; ind != -1; ind = object->vars.tab[ind].next) {
	var = &object->vars.tab[ind];
	if (var->name == name && var->cclass == cclass)
	    return var;
    }

    return NULL;
}

/* Reference-counting kludge: on return, the method's object field has an
   extra reference count, in order to keep it in cache.  objnum must be
   valid. */
/* object_find_method is now a front end that increments the
   cur_search variable and calls object_find_message_recurse,
   the recursive routine.  This is necessary to make single-
   inheritance lineage obejct re-entrant.  It fixes a bug in
   the circular-definition catching logic that caused it to
   fail when a parent's method sent a message to the child as
   a result of a message to teh child hanbdled by the parent
   (whew.) added 5/7/1995 Jeffrey P. kesselman */
method_t *object_find_method(long objnum, long name) {
    Search_params params;
    object_t *object;
    method_t *method, *local_method;
    list_t *parents;
    data_t *d;

    /* Look for cached value. */
    method = method_cache_check(objnum, name, -1);
    if (method)
	return method;

    object = cache_retrieve(objnum);
    parents = list_dup(object->parents);
    cache_discard(object);

    if (list_length(parents) != 0) {
        /* If the object has parents */
        if (list_length(parents) == 1) {
            /* If it has only one parent, call this function recursively. */
            method = object_find_method(list_elem(parents, 0)->u.objnum, name);
        } else {
            /* We've hit a bulge; resort to the reverse depth-first search. */
            cur_search++;
            params.name = name;
            params.stop_at = -1;
            params.done = 0;
            params.last_method_found = NULL;
            for (d = list_last(parents); d; d = list_prev(parents, d))
                search_object(d->u.objnum, &params);
            method = params.last_method_found;
        }
    }

    list_discard(parents);

    /* If we have not found a method defined above, or the top method we
       have found is overridable */
    if (!method || !(method->m_flags & MF_NOOVER)) {
	object = cache_retrieve(objnum);
	local_method = object_find_method_local(object, name);
	if (local_method) {
	    if (method)
		cache_discard(method->object);
	    method = local_method;
	} else {
	    cache_discard(object);
	}
    }

    if (method)
	method_cache_set(objnum, name, -1, method->object->objnum);
    return method;
}

/* Reference-counting kludge: on return, the method's object field has an extra
 * reference count, in order to keep it in cache.  objnum must be valid. */
method_t *object_find_next_method(long objnum, long name, long after)
{
    Search_params params;
    object_t *object;
    method_t *method;
    list_t *parents;
    data_t *d;
    long parent;

    /* Check cache. */
    method = method_cache_check(objnum, name, after);
    if (method)
	return method;

    object = cache_retrieve(objnum);
    parents = object->parents;

    if (list_length(parents) == 1) {
	/* Object has only one parent; search recursively. */
	parent = list_elem(parents, 0)->u.objnum;
	cache_discard(object);
	if (objnum == after)
	    method = object_find_method(parent, name);
	else
	    method = object_find_next_method(parent, name, after);
    } else {
	/* Object has more than one parent; use complicated search. */
	cur_search++;
	params.name = name;
	params.stop_at = (objnum == after) ? -1 : after;
	params.done = 0;
	params.last_method_found = NULL;
	for (d = list_last(parents); d; d = list_prev(parents, d))
	    search_object(d->u.objnum, &params);
	cache_discard(object);
	method = params.last_method_found;
    }

    if (method)
	method_cache_set(objnum, name, after, method->object->objnum);
    return method;
}

/* Perform a reverse depth-first traversal of this object and its ancestors
 * with no repeat visits, thus searching ancestors before children and
 * searching parents right-to-left.  We will take the last method we find,
 * possibly stopping at a method if we were looking for the next method after
 * a given method. */
static void search_object(long objnum, Search_params *params)
{
    object_t *object;
    method_t *method;
    list_t *parents;
    data_t *d;

    object = cache_retrieve(objnum);

    /* Don't search an object twice. */
    if (object->search == cur_search) {
	cache_discard(object);
	return;
    }
    object->dirty = 1;
    object->search = cur_search;

    /* Grab the parents list and discard the object. */
    parents = list_dup(object->parents);
    cache_discard(object);

    /* Traverse the parents list backwards. */
    for (d = list_last(parents); d; d = list_prev(parents, d))
	search_object(d->u.objnum, params);
    list_discard(object->parents);

    /* If the search is done, don't visit this object. */
    if (params->done)
	return;

    /* If we were searching for a next method after a given object, then this
     * might be the given object, in which case we should stop. */
    if (objnum == params->stop_at) {
	params->done = 1;
	return;
    }

    /* Visit this object.  First, get it back from the cache. */
    object = cache_retrieve(objnum);
    method = object_find_method_local(object, params->name);
    if (method) {
	/* We found a method on this object.  Discard the reference count on
	 * the last method found's object, if we have one, and set this method
	 * as the last one found.  Leave object's reference count there, since
	 * we don't want it to get swapped out. */
	if (params->last_method_found)
	    cache_discard(params->last_method_found->object);
	params->last_method_found = method;

	/* If this method is non-overridable, the search is done. */
        if (method->m_flags & MF_NOOVER)
	    params->done = 1;
    } else {
	cache_discard(object);
    }
}

/* Look for a method on an object. */
static method_t *object_find_method_local(object_t *object, long name)
{
    int ind, method;

    /* Traverse hash table thread, stopping if we get a match on the name. */
    ind = hash(ident_name(name)) % object->methods.size;
    method = object->methods.hashtab[ind];
    for (; method != -1; method = object->methods.tab[method].next) {
	if (object->methods.tab[method].m->name == name)
	    return object->methods.tab[method].m;
    }

    return NULL;
}

static method_t *method_cache_check(long objnum, long name, long after)
{
    object_t *object;
    int i;

    i = (10 + objnum + (name << 4) + after) % METHOD_CACHE_SIZE;
    if (method_cache[i].stamp == cur_stamp && method_cache[i].objnum == objnum &&
	method_cache[i].name == name && method_cache[i].after == after &&
	method_cache[i].loc != -1) {
	object = cache_retrieve(method_cache[i].loc);
	return object_find_method_local(object, name);
    } else {
	return NULL;
    }
}

static void method_cache_set(long objnum, long name, long after, long loc)
{
    int i;

    i = (10 + objnum + (name << 4) + after) % METHOD_CACHE_SIZE;
    if (method_cache[i].stamp != 0) {
 /*     write_err("##method_cache_set %d %s", method_cache[i].name, ident_name(method_cache[i].name));*/
      ident_discard(method_cache[i].name);
    }
    method_cache[i].stamp = cur_stamp;
    method_cache[i].objnum = objnum;
    method_cache[i].name = ident_dup(name);
    method_cache[i].after = after;
    method_cache[i].loc = loc;
}

/* this makes me rather wary, hope it works ... */
/* we will know native methods have changed names because the name will
   be different from the one in the initialization table */
int object_rename_method(object_t * object, long oname, long nname) {
    method_t * method;

    method = object_find_method_local(object, oname);
    if (!method)
        return 0;

    method_dup(method);
    object_del_method(object, oname);
    ident_discard(method->name);
    object_add_method(object, nname, method);
    method_discard(method);

    return 1;
}

void object_add_method(object_t *object, long name, method_t *method) {
    int ind, hval;

    /* Invalidate the method cache. */
    cur_stamp++;

    /* Delete the method if it previous existed, calling this on a
       locked method WILL CAUSE PROBLEMS, make sure you check before
       calling this. */
    object_del_method(object, name);

    /* If the method table is full, expand it and its corresponding hash
     * table. */
    if (object->methods.blanks == -1) {
	int new_size, i, ind;

	/* Compute new size and resize tables. */
	new_size = object->methods.size * 2 + MALLOC_DELTA + 1;
	object->methods.tab = EREALLOC(object->methods.tab, struct mptr,
				       new_size);
	object->methods.hashtab = EREALLOC(object->methods.hashtab, int,
					   new_size);

	/* Refill hash table. */
	for (i = 0; i < new_size; i++)
	    object->methods.hashtab[i] = -1;
	for (i = 0; i < object->methods.size; i++) {
	    ind = hash(ident_name(object->methods.tab[i].m->name)) % new_size;
	    object->methods.tab[i].next = object->methods.hashtab[ind];
	    object->methods.hashtab[ind] = i;
	}

	/* Create new thread of blanks and set method pointers to null. */
	for (i = object->methods.size; i < new_size; i++) {
	    object->methods.tab[i].m = NULL;
	    object->methods.tab[i].next = i + 1;
	}
	object->methods.tab[new_size - 1].next = -1;
	object->methods.blanks = object->methods.size;

	object->methods.size = new_size;
    }

    method->object = object;
    method->name = ident_dup(name);

    /* Add method at first blank. */
    ind = object->methods.blanks;
    object->methods.blanks = object->methods.tab[ind].next;
    object->methods.tab[ind].m = method_dup(method);

    /* Add method to hash table thread. */
    hval = hash(ident_name(name)) % object->methods.size;
    object->methods.tab[ind].next = object->methods.hashtab[hval];
    object->methods.hashtab[hval] = ind;

    object->dirty = 1;
}

int object_del_method(object_t *object, long name) {
    int *indp, ind;

    /* Invalidate the method cache. */
    cur_stamp++;

    /* This is the index-thread equivalent of double pointers in a standard
     * linked list.  We traverse the list using pointers to the ->next element
     * of the method pointers. */
    ind = hash(ident_name(name)) % object->methods.size;
    indp = &object->methods.hashtab[ind];
    for (; *indp != -1; indp = &object->methods.tab[*indp].next) {
	ind = *indp;
	if (object->methods.tab[ind].m->name == name) {
            /* check the lock at a higher level, this is a better
               location to put it, but it causes logistic problems */
            /* ack, we found it, but its locked! */
            if (object->methods.tab[ind].m->m_flags & MF_LOCK)
                return -1;

	    /* ok, we can discard it. */
	    method_discard(object->methods.tab[ind].m);
	    object->methods.tab[ind].m = NULL;

	    /* Remove ind from the hash table thread, and add it to the blanks
	     * thread. */
	    *indp = object->methods.tab[ind].next;
	    object->methods.tab[ind].next = object->methods.blanks;
	    object->methods.blanks = ind;

	    object->dirty = 1;

	    /* Return one, meaning the method was successfully deleted. */
	    return 1;
	}
    }

    /* Return zero, meaning no method was found to delete. */
    return 0;
}

list_t *object_list_method(object_t *object, long name, int indent, int parens)
{
    method_t *method;

    method = object_find_method_local(object, name);
    return (method) ? decompile(method, object, indent, parens) : NULL;
}

int object_get_method_flags(object_t * object, long name) {
    method_t * method;

    method = object_find_method_local(object, name);
    return (method) ? method->m_flags : -1;
}

int object_set_method_flags(object_t * object, long name, int flags) {
    method_t * method;

    method = object_find_method_local(object, name);
    if (method == NULL)
        return -1;
    method->m_flags = flags;
    object->dirty = 1;
    return flags;
}

int object_get_method_access(object_t * object, long name) {
    method_t * method;

    method = object_find_method_local(object, name);
    return (method) ? method->m_access : -1;
}

int object_set_method_access(object_t * object, long name, int access) {
    method_t * method;

    method = object_find_method_local(object, name);
    if (method == NULL)
        return -1;
    method->m_access = access;
    object->dirty = 1;
    return access;
}

/* Destroys a method.  Does not delete references from the method's code. */
void method_free(method_t *method)
{
    int i, j;
    Error_list *elist;

    if (method->name != -1)
        ident_discard(method->name);
    if (method->num_args)
	TFREE(method->argnames, method->num_args);
    if (method->num_vars)
	TFREE(method->varnames, method->num_vars);
    TFREE(method->opcodes, method->num_opcodes);
    if (method->num_error_lists) {
	/* Discard identifiers held in the method's error lists. */
	for (i = 0; i < method->num_error_lists; i++) {
	  elist = &method->error_lists[i];
	  for (j = 0; j < elist->num_errors; j++) {
	    ident_discard(elist->error_ids[j]);
	  }
          TFREE(elist->error_ids, elist->num_errors);
	}
	TFREE(method->error_lists, method->num_error_lists);
    }
    efree(method);
}

/* Delete references to object variables and strings in a method's code. */
void method_delete_code_refs(method_t *method)
{
    int i, j, arg_type, opcode;
    Op_info *info;

    for (i = 0; i < method->num_args; i++)
	object_discard_ident(method->object, method->argnames[i]);
    if (method->rest != -1)
	object_discard_ident(method->object, method->rest);

    for (i = 0; i < method->num_vars; i++)
	object_discard_ident(method->object, method->varnames[i]);

    i = 0;
    while (i < method->num_opcodes) {
	opcode = method->opcodes[i];

	/* Use opcode info table for anything else. */
	info = &op_table[opcode];
	for (j = 0; j < 2; j++) {
	    arg_type = (j == 0) ? info->arg1 : info->arg2;
	    if (arg_type) {
		i++;
		switch (arg_type) {

		  case STRING:
		    object_discard_string(method->object, method->opcodes[i]);
		    break;

		  case IDENT:
		    object_discard_ident(method->object, method->opcodes[i]);
		    break;

		}
	    }
	}
	i++;
    }

}

method_t * method_dup(method_t * method) {
    method->refs++;
    return method;
}

void method_discard(method_t *method) {
    method->refs--;
    if (!method->refs) {
	method_delete_code_refs(method);
	method_free(method);
    }
}

int object_del_objname(object_t * object) {
    int result = 0;

    /* if it has one, remove it */
    if (object->objname != -1) {
        result = lookup_remove_name(object->objname);
        ident_discard(object->objname);
    }

    object->objname = -1;

    return result;
}

int object_set_objname(object_t * obj, Ident name) {
    long num;

    /* does it already exist? */
    if (lookup_retrieve_name(name, &num))
        return 0;

    /* do we have a name? Axe it... */
    object_del_objname(obj);

    /* ok, index the new name */
    lookup_store_name(name, obj->objnum);

    /* and for our own purposes, lets remember it */
    obj->objname = ident_dup(name);

    return 1;
}

#undef _object_
