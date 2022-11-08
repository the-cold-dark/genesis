/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Object cache routines.
//
// This code is based on code written by Marcus J. Ranum.  That code, and
// therefore this derivative work, are Copyright (C) 1991, Marcus J. Ranum,
// all rights reserved.
*/

#include "defs.h"

#include "cdc_db.h"
#include "util.h"
#include "execute.h"
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef USE_CLEANER_THREAD
pthread_mutex_t cleaner_lock;
pthread_cond_t cleaner_condition;
pthread_t cleaner;
void *cache_cleaner_worker(void *dummy);

#ifdef DEBUG_BUCKET_LOCK
#define LOCK_BUCKET(func, bucket) \
    write_err("%s: locking %d", func, &dirty[bucket].lock); \
    pthread_mutex_lock(&dirty[bucket].lock); \
    write_err("%s: locked %d", func, &dirty[bucket].lock);
#define UNLOCK_BUCKET(func, bucket) \
    pthread_mutex_unlock(&dirty[bucket].lock); \
    write_err("%s: unlocked %d", func, &dirty[bucket].lock);
#else
#define LOCK_BUCKET(func, bucket) \
    pthread_mutex_lock(&dirty[bucket].lock);
#define UNLOCK_BUCKET(func, bucket) \
    pthread_mutex_unlock(&dirty[bucket].lock);
#endif
#else
#define LOCK_BUCKET(func, bucket)
#define UNLOCK_BUCKET(func, bucket)
#endif


/*
// Store dummy objects for chain heads and tails.  This is a little storage-
// intensive, but it simplifies and speeds up the list operations.
*/

struct cache_buckets {
   Obj *first;
   Obj *last;
};

typedef struct cache_buckets CacheBuckets;
CacheBuckets *active, *inactive;

#ifdef USE_DIRTY_LIST
struct dirty_buckets {
   Obj *first;
   Obj *last;
#ifdef USE_CLEANER_THREAD
   pthread_mutex_t lock;
#endif
};

typedef struct dirty_buckets DirtyBuckets;
DirtyBuckets *dirty;
#endif

#if DEBUG_CACHE
Int        _acounter = 0;
Int        _icounter = 0;
#endif

/* helper functions */

/* add obj to the head of bucket */
static inline void cache_add_to_list_head(CacheBuckets *bucket, Obj *obj)
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
static inline void cache_add_to_list_tail(CacheBuckets *bucket, Obj *obj)
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

static inline void cache_remove_from_list(CacheBuckets *bucket, Obj *obj)
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

#ifdef USE_DIRTY_LIST
static inline void cache_add_to_dirty_list(Obj *obj)
{
    Int ind;

    ind = obj->objnum % cache_width;
    obj->prev_dirty = NULL;
    obj->next_dirty = dirty[ind].first;

    if (dirty[ind].first)
        dirty[ind].first->prev_dirty = obj;

    dirty[ind].first = obj;

    if (dirty[ind].last == NULL)
        dirty[ind].last = obj;
}

inline void cache_dirty_object(Obj *obj)
{
#ifdef USE_CLEANER_THREAD
    Int ind;

    ind = obj->objnum % cache_width;
#endif
    LOCK_BUCKET("cache_dirty_object", ind)

    obj->dirty++;

    if ((cache_watch_object == obj->objnum) &&
        !(obj->dirty % cache_watch_count))
    {
        write_err("Object %s dirtied at:", obj->objname != -1 ? ident_name(obj->objname) : "not named");
        log_current_task_stack(false, write_err);
    }

    if (obj->dirty == 1)
        cache_add_to_dirty_list(obj);

    UNLOCK_BUCKET("cache_dirty_object", ind)
}

static inline void cache_remove_from_dirty(DirtyBuckets *bucket, Obj *obj)
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
#endif

/*
// ----------------------------------------------------------------------
//
// Requires: Shouldn't be called twice.
// Modifies: active, inactive, dirty.
// Effects: Builds an array of object chains in inactive, and an array of
//            empty object chains in active.
//
*/

void init_cache(bool spawn_cleaner)
{
    Obj *obj;
    Int        i, j;

    cache_log_flag      = 0;
    cache_watch_object  = INV_OBJNUM;
    cache_watch_count   = 100;
#ifdef USE_CLEANER_THREAD
    cleaner_wait          = 10;
    cleaner_ignore_dict = dict_new_empty();
#endif
    active              = EMALLOC(CacheBuckets, cache_width);
    inactive            = EMALLOC(CacheBuckets, cache_width);
#ifdef USE_DIRTY_LIST
    dirty               = EMALLOC(DirtyBuckets, cache_width);
#endif

#ifdef USE_CLEANER_THREAD
    pthread_mutex_init(&cleaner_lock, NULL);
    pthread_cond_init(&cleaner_condition, NULL);
    if (spawn_cleaner) {
        if (pthread_create(&cleaner, NULL, cache_cleaner_worker, NULL))
            write_err("init_cache: unable to create cleaner thread");
#ifdef DEBUG_CLEANER_LOCK
        else
            write_err("init_cache: cleaner thread created");
#endif
    }
#endif

    memset(active, 0, sizeof(CacheBuckets)*cache_width);
#ifdef USE_DIRTY_LIST
    memset(dirty, 0, sizeof(DirtyBuckets)*cache_width);
#endif

    for (i = 0; i < cache_width; i++) {
#ifdef USE_CLEANER_THREAD
        pthread_mutex_init(&dirty[i].lock, NULL);
#endif

        /* Inactive list begins as a chain of empty objects. */
        inactive[i].first = inactive[i].last = NULL;
        for (j = 0; j < cache_depth; j++) {
            obj = EMALLOC(Obj, 1);
            obj->objnum = INV_OBJNUM;
#ifdef CLEAN_CACHE
            obj->ucounter=0;
#endif
            obj->dead=0;

            cache_add_to_list_head(&inactive[i], obj);
        }
    }
}

void uninit_cache()
{
    Int i;

#ifdef USE_CLEANER_THREAD
    dict_discard(cleaner_ignore_dict);
#endif
#ifdef USE_DIRTY_LIST
    efree(dirty);
#endif
    for (i = 0; i < cache_width; i++) {
        while (active[i].first) {
            Obj *tmp = active[i].first;
            active[i].first = active[i].first->next_obj;
            if (tmp->objnum != INV_OBJNUM) {
                fprintf(stderr, "object %s($%d) still active!\n",
                        tmp->objname != -1 ? ident_name(tmp->objname) : "not named", tmp->objnum);
                if (tmp->dirty)
                    fprintf(stderr, "and its dirty still!!\n");
                object_free(tmp);
            }
            efree(tmp);
        }

        while (inactive[i].first) {
            Obj *tmp = inactive[i].first;
            inactive[i].first = inactive[i].first->next_obj;
            if (tmp->objnum != INV_OBJNUM) {
                if (tmp->dirty)
                    fprintf(stderr, "object %s($%d) is still dirty!\n",
                            tmp->objname != -1 ? ident_name(tmp->objname) : "not named", tmp->objnum);
                object_free(tmp);
            }
            efree(tmp);
        }
    }
    efree(active);
    efree(inactive);
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.
// Modifies: Contents of active, inactive, database files
// Effects: Returns an object holder linked to the head of the appropriate
//            active chain.  Gets the object holder from the tail of the inactive
//            chain, swapping out the object there if necessary.  If the inactive
//            inactive chain is empty, then we create a new holder.
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
            LOCK_BUCKET("cache_get_holder", ind)
            if (obj->dirty) {
                if (!simble_put(obj, obj->objnum, &obj_size)) {
                    UNLOCK_BUCKET("cache_get_holder", ind)
                    panic("Could not store an object.");
                }
                if (cache_log_flag & CACHE_LOG_OVERFLOW)
                    write_err("cache_get_holder: wrote object %s (size: %d bytes) (dirty: %d)",
                              obj->objname != -1 ? ident_name(obj->objname) : "not named", obj_size, obj->dirty);

                obj->dirty = 0;
#ifdef USE_DIRTY_LIST
                cache_remove_from_dirty(&dirty[ind], obj);
#endif
            }
            UNLOCK_BUCKET("cache_get_holder", ind)
            object_free(obj);
        }

        /* Unlink it from the inactive list, tail still. */
        cache_remove_from_list(&inactive[ind], obj);
    } else {
        /* Allocate a new object. */
        obj = EMALLOC(Obj, 1);
        write_err("cache_get_holder: no holders left, allocating a blank object");
    }

    obj->objnum = objnum;
    obj->search = START_SEARCH_AT;
    obj->dirty = 0;
    obj->dead = 0;
    obj->refs = 1;
#ifdef CLEAN_CACHE
    obj->ucounter = OBJECT_PERSISTENCE;
#endif

    /* we may actually have a connection or file, and when
       it is used these will get set correctly */
    obj->extras = NULL;

#if DEBUG_CACHE
    _acounter++;
#endif

    /* Link the object at the head of the active chain. */
    cache_add_to_list_head(&active[ind], obj);

    return obj;
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.
// Modifies: Contents of active, inactive, database files
// Effects: Returns the object associated with objnum, getting it from the cache
//            or from disk.  If the object is in the inactive chain or is on
//            disk, it will be linked into the active chain.  Returns NULL if no
//            object exists with the given objnum.
//
*/
Obj *cache_retrieve(Long objnum) {
    Int ind = objnum % cache_width;
    Obj *obj;
    Long obj_size;

    if (objnum < 0)
        return NULL;

    /* Search active chain for object. */
    for (obj = active[ind].first; obj; obj = obj->next_obj) {
        if (obj->objnum == objnum) {
            obj->refs++;
#ifdef CLEAN_CACHE
            obj->ucounter += OBJECT_PERSISTENCE;
#endif
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
#ifdef CLEAN_CACHE
            obj->ucounter += OBJECT_PERSISTENCE;
#endif
#if DEBUG_CACHE
            _acounter++;
#endif
            return obj;
        }
    }

    /* Cache miss.  Find an object to load in from disk. */
    obj = cache_get_holder(objnum);

    /* Read the object into the place-holder, if it's on disk. */
    LOCK_BUCKET("cache_retrieve", ind)
    if (!simble_get(obj, objnum, &obj_size)) {
        /* Oops.  add back to inactive list tail*/
        obj->objnum = INV_OBJNUM;
        cache_remove_from_list(&active[ind], obj);
        cache_add_to_list_tail(&inactive[ind], obj);
        obj = NULL;
    }
    UNLOCK_BUCKET("cache_retrieve", ind)
    if (obj && cache_log_flag & CACHE_LOG_READ)
        write_err("cache_retrieve: read object %s (size: %d bytes)",
                  obj->objname != -1 ? ident_name(obj->objname) : "not named", obj_size);
#ifdef USE_PARENT_OBJS
    if (obj)
        object_load_parent_objs(obj);
#endif
    return obj;
}

/*
// ----------------------------------------------------------------------
*/

Obj *cache_grab(Obj *obj) {
    obj->refs++;
#ifdef CLEAN_CACHE
    obj->ucounter += OBJECT_PERSISTENCE;
#endif
    return obj;
}

/*
// ----------------------------------------------------------------------
//
// Requires: Initialized cache.  obj should point to an active object.
// Modifies: obj, contents of active and inactive, database files.
// Effects: Decreases the refcount on obj, unlinking it from the active chain
//            if the refcount hits zero.  If the object is marked dead, then it
//            is destroyed when it is unlinked from the active chain.
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
           long as obj isn't in any chains at the time of simble_del(). */
        object_destroy(obj);
        simble_del(obj->objnum);

        LOCK_BUCKET("cache_discard", ind)

        obj->dirty = 0;
#ifdef USE_DIRTY_LIST
        cache_remove_from_dirty(&dirty[ind], obj);
#endif

        UNLOCK_BUCKET("cache_discard", ind)

        obj->objnum = INV_OBJNUM;
        cache_add_to_list_tail(&inactive[ind], obj);
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

bool cache_check(Long objnum) {
    Int ind = objnum % cache_width;
    Obj *obj;

    if (objnum < 0)
        return false;

    /* Search active chain. */
    for (obj = active[ind].first; obj; obj = obj->next_obj) {
        if (obj->objnum == objnum)
            return true;
    }

    /* Search inactive chain. */
    for (obj = inactive[ind].first; obj; obj = obj->next_obj) {
        if (obj->objnum == objnum)
            return true;
    }

    /* Check database on disk. */
    return simble_check(objnum);
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
#ifndef USE_DIRTY_LIST
    Int j;
#else
    Obj *tobj;
#endif
    Obj *obj;
    Long obj_size;

#ifdef USE_CLEANER_THREAD
#ifdef DEBUG_CLEANER_LOCK
    write_err("cache_sync: locking cleaner");
#endif
    pthread_mutex_lock(&cleaner_lock);
#ifdef DEBUG_CLEANER_LOCK
    write_err("cache_sync: locked cleaner");
#endif
#endif
    /* Traverse all the active and inactive chains. */
    if (cache_log_flag & CACHE_LOG_SYNC)
        write_err("cache_sync: start of sync");
    for (i = 0; i < cache_width; i++) {
        /* Check active chain. */
        LOCK_BUCKET("cache_sync", i)

#ifndef USE_DIRTY_LIST
        for (j=0; j<2; j++)
        {
            if (j == 0)
                obj = active[i].first;
            else
                obj = inactive[i].first;
#else
            obj = dirty[i].first;
#endif
            while (obj) {
#ifndef USE_DIRTY_LIST
                if (obj->objnum != INV_OBJNUM && obj->dirty) {
#endif
                    if (obj->dead) {
                        if (cache_log_flag & CACHE_LOG_DEAD_WRITE)
                            write_err("cache_sync: skipping dead object");
                    } else {
                        if (!simble_put(obj, obj->objnum, &obj_size)) {
                            UNLOCK_BUCKET("cache_sync", i)
                            panic("Could not store an object.");
                        }
                        if (cache_log_flag & CACHE_LOG_SYNC)
                            write_err("cache_sync: wrote object %s (size: %d bytes) (dirty: %d)",
                                      obj->objname != -1 ? ident_name(obj->objname) : "not named", obj_size, obj->dirty);
                        obj->dirty = 0;
                    }
#ifndef USE_DIRTY_LIST
                }
#endif
#ifdef USE_DIRTY_LIST
                tobj = obj->next_dirty;
                obj->next_dirty = obj->prev_dirty = NULL;
                obj = tobj;
#else
                obj = obj->next_obj;
#endif
            }
#ifndef USE_DIRTY_LIST
        }
#else
        dirty[i].first = dirty[i].last = NULL;
#endif
        UNLOCK_BUCKET("cache_sync", i)
    }

    simble_flush();
#ifdef USE_CLEANER_THREAD
    pthread_mutex_unlock(&cleaner_lock);
#ifdef DEBUG_CLEANER_LOCK
    write_err("cache_sync: unlocked cleaner");
#endif
#endif
}

#ifdef USE_CLEANER_THREAD
void *cache_cleaner_worker(void *dummy)
{
    Int     cache_bucket = 0,
            start_bucket,
            wrote_something;
    Obj   * tobj,
          * tobj2;
    Long    obj_size;
    cData   cthis;
    int status;
    struct timeval now;
    struct timespec time_to_wait;

    /* Grab this so that it is locked when the cond_timedwait runs */
    pthread_mutex_lock(&cleaner_lock);

    while (running) {
        gettimeofday(&now, NULL);
        time_to_wait.tv_sec = now.tv_sec + cleaner_wait;
        time_to_wait.tv_nsec = now.tv_usec * 1000;

#ifdef DEBUG_CLEANER_LOCK
        write_err("cache_cleaner_worker: begin cond_timedwait");
#endif
        status = pthread_cond_timedwait(&cleaner_condition,
                                        &cleaner_lock,
                                        &time_to_wait);
#ifdef DEBUG_CLEANER_LOCK
        if (status == 0) {
            write_err("cache_cleaner_worker: condition was signaled");
        }
        write_err("cache_cleaner_worker: end cond_timedwait");
#endif

        start_bucket = cache_bucket;
        wrote_something = 0;
        cthis.type = OBJNUM;
        do {
            LOCK_BUCKET("cache_cleaner_worker", cache_bucket)
            tobj = dirty[cache_bucket].first;
            while (tobj) {
                cthis.u.objnum = tobj->objnum;
                if (tobj->refs == 0 &&
                    !dict_contains(cleaner_ignore_dict, &cthis)) {
                    if (tobj->dead) {
                        if (cache_log_flag & CACHE_LOG_DEAD_WRITE)
                            write_err("cache_cleaner_worker: skipping dead object");
                    } else {
                        wrote_something = 1;
                        if (!simble_put(tobj, tobj->objnum, &obj_size)) {
                            UNLOCK_BUCKET("cache_cleaner_worker", cache_bucket)
                            panic("Could not store an object.");
                        }
                        if (cache_log_flag & CACHE_LOG_SYNC)
                            write_err("cache_cleaner_worker: wrote object %s (size: %d bytes) (dirty: %d)",
                                      tobj->objname != -1 ? ident_name(tobj->objname) : "not named", obj_size, tobj->dirty);
                        tobj->dirty = 0;
                    }

                    tobj2 = tobj->next_dirty;
                    cache_remove_from_dirty(&dirty[cache_bucket], tobj);
                    tobj = tobj2;
                }
                else
                    tobj = tobj->next_dirty;
            }

            UNLOCK_BUCKET("cache_cleaner_worker", cache_bucket)

            if (++cache_bucket == cache_width)
                cache_bucket = 0;
        } while (!wrote_something && cache_bucket != start_bucket);
    }

    pthread_mutex_unlock(&cleaner_lock);

    return NULL;
}
#endif

#ifdef DRIVER_DEBUG
/*
// ----------------------------------------------------------------------
//
// Called during main loop to verify that no objects are active,
// or if they are, it is only because they are paused or suspended.
//
*/

/** disabled since it doesn't work */
/* NOTE: NOT well checked, updated to match current variable names and
 *       structure of the active list, but it might I think it might be
 *       buggy.  Does it need to walk up the stack and check every frame?
 *       How about the frame's method->obj?
 */
void cache_sanity_check(void) {
#if DISABLED
    Int       i;
    Obj     * obj;
    VMState * task;

    for (i = 0; i < cache_width; i++) {
        for (obj = active[i].first; obj; obj = obj->next_obj) {

            /* check suspended tasks */
            for (task = suspended; task != NULL; task = task->next) {
                if (task->cur_frame->object->objnum == obj->objnum)
                    goto end;
            }

            /* check preempted tasks */
            for (task = preempted; task != NULL; task = task->next) {
                if (task->cur_frame->object->objnum == obj->objnum)
                    goto end;
            }

            /* ack, panic */
            panic("Active object #%d at start of main loop.", (Int) obj->objnum);

            /* label both for loops can jump to, skipping the panic */
end:
            ;
        }
    }
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
        for (obj = inactive[i].first; obj; obj = obj->next_obj) {
            obj->ucounter >>= 1;
            if (obj->ucounter > 0)
                continue;
            if (obj->objnum != INV_OBJNUM && obj->dirty) {
                if (!simble_put(obj, obj->objnum, &obj_size))
                    panic("Could not store an object.");
                if (cache_log_flag & CACHE_LOG_CLEANUP)
                    write_err("cache_cleanup: wrote object %s (size: %d bytes) (dirty: %d)",
                              obj->objname != -1 ? ident_name(obj->objname) : "not named", obj_size, obj->dirty);
                obj->dirty = 0;
            }
            if (obj->objnum != INV_OBJNUM) {
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

cList * cache_info(void) {
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
