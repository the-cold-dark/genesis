/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Object cache routines.
//
// This code is based on code written by Marcus J. Ranum.  That code, and
// therefore this derivative work, are Copyright (C) 1991, Marcus J. Ranum,
// all rights reserved.
*/

#define _cache_

#include "defs.h"

#include "cdc_db.h"
#include "util.h"
#include "execute.h"

/*
// Store dummy objects for chain heads and tails.  This is a little storage-
// intensive, but it simplifies and speeds up the list operations.
*/

Obj * active;
Obj * inactive;

#if DEBUG_CACHE
Int        _acounter = 0;
Int        _icounter = 0;
#endif

/*
// ----------------------------------------------------------------------
//
// Requires: Shouldn't be called twice.
// Modifies: active, inactive.
// Effects: Builds an array of object chains in inactive, and an array of
//	    empty object chains in active.
//
*/

void init_cache(void) {
    Obj *obj;
    Int	i, j;

    active = EMALLOC(Obj, cache_width);
    inactive = EMALLOC(Obj, cache_width);

    for (i = 0; i < cache_width; i++) {
	/* Active list starts out empty. */
	active[i].next = active[i].prev = &active[i];

	/* Inactive list begins as a chain of empty objects. */
	inactive[i].next = inactive[i].prev = &inactive[i];
	for (j = 0; j < cache_depth; j++) {
	    obj = EMALLOC(Obj, 1);
	    obj->objnum = INV_OBJNUM;
            obj->ucounter=0;
	    obj->prev = &inactive[i];
	    obj->next = inactive[i].next;
	    obj->prev->next = obj->next->prev = obj;
	}
    }
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.
// Modifies: Contents of active, inactive, database files
// Effects: Returns an object holder linked to the head of the appropriate
//	    active chain.  Gets the object holder from the tail of the inactive
//	    chain, swapping out the object there if necessary.  If the inactive
//	    inactive chain is empty, then we create a new holder.
//
*/

Obj * cache_get_holder(Long objnum) {
    Int ind = objnum % cache_width;
    Obj *obj;

    if (inactive[ind].next != &inactive[ind]) {
	/* Use the object at the tail of the inactive list. */
	obj = inactive[ind].prev;

	/* Check if we need to swap anything out. */
	if (obj->objnum != INV_OBJNUM) {
	    if (obj->dirty) {
		if (!db_put(obj, obj->objnum))
		    panic("Could not store an object.");
	    }
	    object_free(obj);
	}

	/* Unlink it from the inactive list. */
	obj->prev->next = obj->next;
	obj->next->prev = obj->prev;
    } else {
	/* Allocate a new object. */
	obj = EMALLOC(Obj, 1);
    }

    /* Link the object a the head of the active chain. */
    obj->prev = &active[ind];
    obj->next = active[ind].next;
    obj->prev->next = obj->next->prev = obj;

    obj->search = START_SEARCH_AT;
    obj->dirty = 0;
    obj->dead = 0;
    obj->refs = 1;
    obj->ucounter = OBJECT_PERSISTANCE;

    /* we may actually have a connection or file, and when
       it is used these will get set correctly */
    obj->conn = NULL;
    obj->file = NULL;

#if DEBUG_CACHE
    _acounter++;
#endif

    obj->objnum = objnum;
    return obj;
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.
// Modifies: Contents of active, inactive, database files
// Effects: Returns the object associated with objnum, getting it from the cache
//	    or from disk.  If the object is in the inactive chain or is on
//	    disk, it will be linked into the active chain.  Returns NULL if no
//	    object exists with the given objnum.
//
*/
Obj *cache_retrieve(Long objnum) {
    Int ind = objnum % cache_width;
    Obj *obj;

    if (objnum < 0)
	return NULL;

    /* Search active chain for object. */
    for (obj = active[ind].next; obj != &active[ind]; obj = obj->next) {
	if (obj->objnum == objnum) {
	    obj->refs++;
            obj->ucounter+=OBJECT_PERSISTANCE;
	    return obj;
	}
    }

    /* Search inactive chain for object. */
    for (obj = inactive[ind].next; obj != &inactive[ind]; obj = obj->next) {
	if (obj->objnum == objnum) {
	    /* Remove object from inactive chain. */
	    obj->next->prev = obj->prev;
	    obj->prev->next = obj->next;

	    /* Install object at head of active chain. */
#if DEBUG_CACHE
            _icounter--;
#endif
	    obj->prev = &active[ind];
	    obj->next = active[ind].next;
	    obj->prev->next = obj->next->prev = obj;

	    obj->refs = 1;
            obj->ucounter+=OBJECT_PERSISTANCE;
#if DEBUG_CACHE
            _acounter++;
#endif
	    return obj;
	}
    }

    /* Cache miss.  Find an object to load in from disk. */
    obj = cache_get_holder(objnum);

    /* Read the object into the place-holder, if it's on disk. */
    if (db_get(obj, objnum)) {
	return obj;
    } else {
	/* Oops.  Install holder at tail of inactive chain and return NULL. */
	obj->objnum = INV_OBJNUM;
	obj->prev->next = obj->next;
	obj->next->prev = obj->prev;
#if 1
	obj->prev = inactive[ind].prev;
	obj->next = &inactive[ind];
	obj->prev->next = obj->next->prev = obj;
#else
        efree(obj);
#endif
	return NULL;
    }
}

/*
// ----------------------------------------------------------------------
*/

Obj *cache_grab(Obj *obj) {
    obj->refs++;
    obj->ucounter+=OBJECT_PERSISTANCE;
    return obj;
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.  obj should point to an active object.
// Modifies: obj, contents of active and inactive, database files.
// Effects: Decreases the refcount on obj, unlinking it from the active chain
//	    if the refcount hits zero.  If the object is marked dead, then it
//	    is destroyed when it is unlinked from the active chain.
//
*/

void cache_discard(Obj *obj) {
    Int ind;

    if (!obj)
      return;

    /* Decrease reference count. */
    obj->refs--;
    if (obj->refs)
	return;

#if DEBUG_CACHE
    _acounter--;
#endif
    ind = obj->objnum % cache_width;

    /* Reference count hit 0; remove from active chain. */
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;

    if (obj->dead) {
	/* The object is dead; remove it from the database, and install the
           holder at the tail of the inactive chain.  Be careful about this,
           since object_destroy() can fiddle with the cache.  We're safe as
           long as obj isn't in any chains at the time of db_del(). */
	db_del(obj->objnum);
	object_destroy(obj);
	obj->objnum = INV_OBJNUM;
	obj->prev = inactive[ind].prev;
	obj->next = &inactive[ind];
	obj->prev->next = obj->next->prev = obj;
    } else {
	/* Install at head of inactive chain. */
	obj->prev = &inactive[ind];
	obj->next = inactive[ind].next;
	obj->prev->next = obj->next->prev = obj;
#if DEBUG_CACHE
        _icounter++;
#endif
    }
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.
// Effects: Returns nonzero if an object exists with the given objnum.
//
*/

Int cache_check(Long objnum) {
    Int ind = objnum % cache_width;
    Obj *obj;

    if (objnum < 0)
	return 0;

    /* Search active chain. */
    for (obj = active[ind].next; obj != &active[ind]; obj = obj->next) {
	if (obj->objnum == objnum)
	    return 1;
    }

    /* Search inactive chain. */
    for (obj = inactive[ind].next; obj != &inactive[ind]; obj = obj->next) {
	if (obj->objnum == objnum)
	    return 1;
    }

    /* Check database on disk. */
    return db_check(objnum);
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.
// Modifies: Database files.
// Effects: Writes out all objects in the cache which are marked dirty.
//
*/

void cache_sync(void) {
    Int i;
    Obj *obj;

    /* Traverse all the active and inactive chains. */
    for (i = 0; i < cache_width; i++) {
	/* Check active chain. */
	for (obj = active[i].next; obj != &active[i]; obj = obj->next) {
	    if (obj->dirty) {
                if (!db_put(obj, obj->objnum))
		    panic("Could not store an object.");
		obj->dirty = 0;
	    }
	}

	/* Check inactive chain. */
	for (obj = inactive[i].next; obj != &inactive[i]; obj = obj->next) {
	    if (obj->objnum != INV_OBJNUM && obj->dirty) {
                if (!db_put(obj, obj->objnum))
		    panic("Could not store an object.");
		obj->dirty = 0;
	    }
	}
    }

    db_flush();
}

/*
// ----------------------------------------------------------------------
*/

Obj *cache_first(void) {
    Long objnum;

    cache_sync();
    objnum = lookup_first_objnum();
    if (objnum == INV_OBJNUM)
	return NULL;
    return cache_retrieve(objnum);
}

/*
// ----------------------------------------------------------------------
*/

Obj *cache_next(void) {
    Long objnum;

    objnum = lookup_next_objnum();
    if (objnum == INV_OBJNUM)
	return NULL;
    return cache_retrieve(objnum);
}

/*
// ----------------------------------------------------------------------
//
// Called during main loop to verify that no objects are active,
// or if they are, it is only because they are paused or suspended.
//
*/

void cache_sanity_check(void) {
#if DISABLED /* need to do some more work here */
    Int        i;
    Obj * obj;
    VMState  * task;

    /* using labels was the best way I could come up with, I'm sorry... */
    for (i = 0; i < cache_width; i++) {
        for (obj = active[i].next; obj != &active[i]; obj = obj->next) {

            /* check suspended tasks */
            for (task = tasks; task != NULL; task = task->next) {
                if (task->cur_frame->object->objnum == obj->objnum)
                    goto end;
            }

            /* check paused tasks */
            for (task = paused; task != NULL; task = task->next) {
                if (task->cur_frame->object->objnum == obj->objnum)
                    goto end;
            }

            /* ack, panic */
	    panic("Active object #%d at start of main loop.", (Int) obj->objnum);

            /* label both for loops can jump to, skipping the panic */
            end:
        }
    }
#endif
#if 0
	if (active[i].next != &active[i]) 
	    panic("Active objects at start of main loop.");
#endif
}

/*
// ----------------------------------------------------------------------
//
// Called during main loop to clean inactive objects from the cache
//
*/

#ifdef CLEAN_CACHE
void cache_cleanup(void) {
    Obj * obj;
    Int        i;

    for (i = 0; i < cache_width; i++) {
        for (obj = inactive[i].next; obj != &inactive[i]; obj = obj->next) {
            obj->ucounter >>= 1;
            if (obj->ucounter > 0)
                continue;
            if (obj->objnum != INV_OBJNUM && obj->dirty) {
                if (!db_put(obj, obj->objnum))
                    panic("Could not store an object.");
                obj->dirty = 0;
            }
            if(obj->objnum != INV_OBJNUM) {
#if DEBUG_CACHE
                _icounter--;
                fprintf(errfile,"<%d\n",_icounter);
#endif
                object_free(obj);
                obj->objnum = INV_OBJNUM;
                continue;
            }
        }
    }
}
#endif

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.
// Effects: returns a list mapping out the current state of the cache
//
// Returned list will always be:
//
//    [WIDTH, DEPTH, ...]
//
// where ... is currently a list of strings, where each string contains
// characters representing objects, as:
//
//    a=active to current task
//    A=active and dirty
//    i=inactive
//    I=inactive and dirty
//
// -Brandon
*/

cList * cache_info(int level) {
    int     x;
    Obj   * obj;
    cList * out;
    cList * list;
    cData * d;
    cStr  * str;

    out = list_new(3);
    list = list_new(cache_width);
    d = list_empty_spaces(out, 3);
    d[0].type = INTEGER;
    d[0].u.val = cache_width;
    d[1].type = INTEGER;
    d[1].u.val = cache_depth;
    d[2].type = LIST;
    d[2].u.list = list;
    d = list_empty_spaces(list, cache_width);

    for (x=0; x < cache_width; x++) {
        str = string_new(cache_depth);
        for (obj = active[x].next; obj != &active[x]; obj = obj->next) {
            if (obj->objnum != INV_OBJNUM) {
                if (obj->dirty)
                    str = string_addc(str, 'A');
                else
                    str = string_addc(str, 'a');
            }
        }
        for (obj = inactive[x].next; obj != &inactive[x]; obj = obj->next) {
            if (obj->objnum != INV_OBJNUM) {
                if (obj->dirty)
                    str = string_addc(str, 'I');
                else
                    str = string_addc(str, 'i');
             }
        }
        d[x].type = STRING;
        d[x].u.str = str;
    }

    return out;
}
#undef _cache_
