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

struct cache_buckets {
   Obj *first;
   Obj *last;
};

typedef struct cache_buckets CacheBuckets;

CacheBuckets *active, *inactive, *dirty;

#if DEBUG_CACHE
Int        _acounter = 0;
Int        _icounter = 0;
#endif

/* helper functions */

/* add obj to the head of bucket */
static void cache_add_to_list_head(CacheBuckets *bucket, Obj *obj)
{
    obj->prev_obj = NULL;
    obj->next_obj = bucket->first;

    /* redundant? */
    if (bucket->first)
	bucket->first->prev_obj = obj;

    bucket->first = obj;

    if (bucket->last == NULL)
	bucket->last = obj;
}

/* add obj to the tail of bucket */
static void cache_add_to_list_tail(CacheBuckets *bucket, Obj *obj)
{
    obj->next_obj = NULL;
    obj->prev_obj = bucket->last;

    /* redundant? */
    if (bucket->last)
	bucket->last->next_obj = obj;

    bucket->last = obj;

    if (bucket->first == NULL)
	bucket->first = obj;
}

static void cache_remove_from_list(CacheBuckets *bucket, Obj *obj)
{
    if (obj->next_obj)
        obj->next_obj->prev_obj = obj->prev_obj;
    if (obj->prev_obj)
        obj->prev_obj->next_obj = obj->next_obj;
    if (obj == bucket->first)
	bucket->first = obj->next_obj;
    if (obj == bucket->last)
	bucket->last = obj->prev_obj;
    obj->next_obj = obj->prev_obj = NULL;
}

inline void cache_dirty_object(Obj *obj)
{
    Int ind;

    obj->dirty++;
    if ((cache_watch_object == obj->objnum) && !(obj->dirty % cache_watch_count))
    {
	cList *stack = task_stack(cur_frame, FALSE);
	log_task_stack(task_id, stack, write_err);
    }
    if (obj->dirty > 1)
	return;

    ind = obj->objnum % cache_width;
    obj->prev_dirty = NULL;
    obj->next_dirty = dirty[ind].first;

    if (dirty[ind].first)
	dirty[ind].first->prev_dirty = obj;

    dirty[ind].first = obj;

    if (dirty[ind].last == NULL)
	dirty[ind].last = obj;
}

static void cache_remove_from_dirty(CacheBuckets *bucket, Obj *obj)
{
    if (obj->next_dirty)
        obj->next_dirty->prev_dirty = obj->prev_dirty;
    if (obj->prev_dirty)
        obj->prev_dirty->next_dirty = obj->next_dirty;
    if (obj == bucket->first)
	bucket->first = obj->next_dirty;
    if (obj == bucket->last)
	bucket->last = obj->prev_dirty;
    obj->next_dirty = obj->prev_dirty = NULL;
}

/*
// ----------------------------------------------------------------------
//
// Requires: Shouldn't be called twice.
// Modifies: active, inactive, dirty.
// Effects: Builds an array of object chains in inactive, and an array of
//	    empty object chains in active.
//
*/

void init_cache(void) {
    Obj *obj;
    Int	i, j;

    cache_log_flag     = 0;
    cache_watch_object = INV_OBJNUM;
    cache_watch_count  = 100;
    active             = EMALLOC(CacheBuckets, cache_width);
    inactive           = EMALLOC(CacheBuckets, cache_width);
    dirty              = EMALLOC(CacheBuckets, cache_width);

    for (i = 0; i < cache_width; i++) {
	/* Active list starts out empty. */
	active[i].first = active[i].last = NULL;
	dirty[i].first  = dirty[i].last  = NULL;

	/* Inactive list begins as a chain of empty objects. */
	inactive[i].first = inactive[i].last = NULL;
	for (j = 0; j < cache_depth; j++) {
	    obj = EMALLOC(Obj, 1);
	    obj->objnum = INV_OBJNUM;
            obj->ucounter=0;

	    cache_add_to_list_head(&inactive[i], obj);
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
    Long obj_size;

    if (inactive[ind].last) {
	/* Use the object at the tail of the inactive list. */
	obj = inactive[ind].last;

	/* Check if we need to swap anything out. */
	if (obj->objnum != INV_OBJNUM) {
	    if (obj->dirty) {
		if (!db_put(obj, obj->objnum, &obj_size))
		    panic("Could not store an object.");
                if (cache_log_flag & CACHE_LOG_OVERFLOW)
		    write_err("cache_get_holder: wrote object %s (size: %d bytes) (dirty: %d)", ident_name(obj->objname), obj_size, obj->dirty);

		cache_remove_from_dirty(&dirty[ind], obj);
	    }
	    object_free(obj);
	}

	/* Unlink it from the inactive list, tail still. */
	cache_remove_from_list(&inactive[ind], obj);
    } else {
	/* Allocate a new object. */
	obj = EMALLOC(Obj, 1);
	write_err("cache_get_holder: no holders left, allocating a blank object");
    }

    /* Link the object a the head of the active chain. */
    cache_add_to_list_head(&active[ind], obj);

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
    for (obj = active[ind].first; obj; obj = obj->next_obj) {
	if (obj->objnum == objnum) {
	    obj->refs++;
            obj->ucounter+=OBJECT_PERSISTANCE;
	    return obj;
	}
    }

    /* Search inactive chain for object. */
    for (obj = inactive[ind].first; obj; obj = obj->next_obj) {
	if (obj->objnum == objnum) {
	    cache_remove_from_list(&inactive[ind], obj);

#if DEBUG_CACHE
            _icounter--;
#endif
	    /* Install object at head of active chain. */
	    cache_add_to_list_head(&active[ind], obj);

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
	/* Oops.  add back to inactive list tail*/
	obj->objnum = INV_OBJNUM;
	cache_remove_from_list(&active[ind], obj);
	cache_add_to_list_tail(&inactive[ind], obj);
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
    cache_remove_from_list(&active[ind], obj);

    if (obj->dead) {
	/* The object is dead; remove it from the database, and install the
           holder at the tail of the inactive chain.  Be careful about this,
           since object_destroy() can fiddle with the cache.  We're safe as
           long as obj isn't in any chains at the time of db_del(). */
	db_del(obj->objnum);
	object_destroy(obj);
	obj->objnum = INV_OBJNUM;
	cache_add_to_list_tail(&inactive[ind], obj);
	cache_remove_from_dirty(&dirty[ind], obj);
    } else {
	/* Install at head of inactive chain. */
	cache_add_to_list_head(&inactive[ind], obj);
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
    for (obj = active[ind].first; obj; obj = obj->next_obj) {
	if (obj->objnum == objnum)
	    return 1;
    }

    /* Search inactive chain. */
    for (obj = inactive[ind].first; obj; obj = obj->next_obj) {
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
    Obj *obj, *tobj;
    Long obj_size;

    /* Traverse all the active and inactive chains. */
    if (cache_log_flag & CACHE_LOG_SYNC)
        write_err("cache_sync: start of sync");
    for (i = 0; i < cache_width; i++) {
	/* Check active chain. */
	obj = dirty[i].first;
	while (obj)
	{
            if (!db_put(obj, obj->objnum, &obj_size))
		panic("Could not store an object.");
            if (cache_log_flag & CACHE_LOG_SYNC)
		write_err("cache_sync: wrote object %s (size: %d bytes) (dirty: %d)", ident_name(obj->objname), obj_size, obj->dirty);
	    obj->dirty = 0;

	    tobj = obj->next_dirty;
	    obj->next_dirty = obj->prev_dirty = NULL;
	    obj = tobj;
	}
	dirty[i].first = dirty[i].last = NULL;
    }

    db_flush();
}

#if 0
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
#endif

/*
// ----------------------------------------------------------------------
//
// Called during main loop to clean inactive objects from the cache
//
*/

#ifdef CLEAN_CACHE
void cache_cleanup(void) {
    Obj * obj;
    Int   i;
    Long  obj_size;

    for (i = 0; i < cache_width; i++) {
        for (obj = inactive[i].next; obj != &inactive[i]; obj = obj->next) {
            obj->ucounter >>= 1;
            if (obj->ucounter > 0)
                continue;
            if (obj->objnum != INV_OBJNUM && obj->dirty) {
                if (!db_put(obj, obj->objnum, &obj_size))
                    panic("Could not store an object.");
		if (cache_log_flag & CACHE_LOG_CLEANUP)
		    write_err("cache_cleanup: wrote object %s (size: %d bytes) (dirty: %d)", ident_name(obj->objname), obj_size, obj->dirty);
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
        for (obj = active[x].first; obj; obj = obj->next_obj) {
            if (obj->objnum != INV_OBJNUM) {
                if (obj->dirty)
                    str = string_addc(str, 'A');
                else
                    str = string_addc(str, 'a');
            }
        }
        for (obj = inactive[x].first; obj; obj = obj->next_obj) {
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
