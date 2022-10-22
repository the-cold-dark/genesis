/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Interface to dbm index of object locations.
*/

#include "defs.h"

#include <sys/types.h>
#ifdef __UNIX__
#include <sys/file.h>
#endif
#include <sys/stat.h>

#include <fcntl.h>
#include <string.h>

#ifdef USE_CLEANER_THREAD
pthread_mutex_t lookup_mutex;

#ifdef DEBUG_LOOKUP_LOCK
#define LOCK_LOOKUP(func) do { \
        write_err("%s: locking db", func); \
        pthread_mutex_lock(&lookup_mutex); \
        write_err("%s: locked db", func); \
    } while(0)
#define UNLOCK_LOOKUP(func) do {\
        pthread_mutex_unlock(&lookup_mutex); \
        write_err("%s: unlocked db", func); \
    } while(0)
#else
#define LOCK_LOOKUP(func) do { \
        pthread_mutex_lock(&lookup_mutex); \
    } while(0)
#define UNLOCK_LOOKUP(func) do { \
        pthread_mutex_unlock(&lookup_mutex); \
    } while(0)
#endif
#else
#define LOCK_LOOKUP(func)
#define UNLOCK_LOOKUP(func)
#endif

#include "cdc_db.h"
#include "util.h"

#include <ndbm.h>

#ifdef S_IRUSR
#define READ_WRITE                (S_IRUSR | S_IWUSR)
#define READ_WRITE_EXECUTE        (S_IRUSR | S_IWUSR | S_IXUSR)
#else
#define READ_WRITE 0600
#define READ_WRITE_EXECUTE 0700
#endif

Int name_cache_hits = 0;
Int name_cache_misses = 0;

static datum objnum_key(cObjnum objnum, Number_buf nbuf);
static datum name_key(Ident name);
static datum offset_size_value(off_t offset, Int size, Number_buf nbuf);
static void parse_offset_size_value(datum value, off_t *offset, Int *size);
static datum objnum_value(cObjnum objnum, Number_buf nbuf);
static void sync_name_cache(void);
static Int store_name(Ident name, cObjnum objnum);
static Int get_name(Ident name, cObjnum *objnum);

static DBM *dbp;

struct name_cache_entry {
    Ident   name;
    cObjnum objnum;
    char    dirty;
    char    on_disk;
} name_cache[NAME_CACHE_SIZE + 1];

void lookup_open(char *name, Int cnew) {
    Int i;

#ifdef USE_CLEANER_THREAD
    pthread_mutex_init(&lookup_mutex, NULL);
#endif

    if (cnew)
        dbp = dbm_open(name, O_TRUNC | O_RDWR | O_CREAT | O_BINARY, READ_WRITE);
    else
        dbp = dbm_open(name, O_RDWR | O_BINARY, READ_WRITE);
    if (!dbp)
        fail_to_start("Cannot open dbm database file.");

    for (i = 0; i < NAME_CACHE_SIZE; i++)
        name_cache[i].name = NOT_AN_IDENT;
}

void lookup_close(void) {
    sync_name_cache();
    dbm_close(dbp);
}

void lookup_sync(void) {
    char buf[255];

    sprintf(buf, "%s/index", c_dir_binary);

    LOCK_LOOKUP("lookup_sync");

    /* Only way to do this with ndbm is close and re-open. */
    sync_name_cache();
    dbm_close(dbp);
    dbp = dbm_open(buf, O_RDWR | O_CREAT | O_BINARY, READ_WRITE);

    UNLOCK_LOOKUP("lookup_sync");

    if (!dbp)
        panic("Cannot reopen dbm database file.");
}

Int lookup_retrieve_objnum(cObjnum objnum, off_t *offset, Int *size)
{
    datum key, value;
    Number_buf nbuf;

    LOCK_LOOKUP("lookup_retrieve_objnum");

    /* Get the value for objnum from the database. */
    key = objnum_key(objnum, nbuf);
    value = dbm_fetch(dbp, key);
    if (!value.dptr)
    {
        UNLOCK_LOOKUP("lookup_retrieve_objnum");
        return 0;
    }

    parse_offset_size_value(value, offset, size);
    UNLOCK_LOOKUP("lookup_retrieve_objnum");
    return 1;
}

Int lookup_store_objnum(cObjnum objnum, off_t offset, Int size)
{
    datum key, value;
    Number_buf nbuf1, nbuf2;

    LOCK_LOOKUP("lookup_store_objnum");
    key = objnum_key(objnum, nbuf1);
    value = offset_size_value(offset, size, nbuf2);
    if (dbm_store(dbp, key, value, DBM_REPLACE)) {
        write_err("ERROR: Failed to store key %l.", objnum);
        UNLOCK_LOOKUP("lookup_store_objnum");
        return 0;
    }

    UNLOCK_LOOKUP("lookup_store_objnum");
    return 1;
}

Int lookup_remove_objnum(cObjnum objnum)
{
    datum key;
    Number_buf nbuf;

    LOCK_LOOKUP("lookup_remove_objnum");
    /* Remove the key from the database. */
    key = objnum_key(objnum, nbuf);
    if (dbm_delete(dbp, key)) {
        write_err("ERROR: Failed to delete key %l.", objnum);
        UNLOCK_LOOKUP("lookup_remove_objnum");
        return 0;
    }
    UNLOCK_LOOKUP("lookup_remove_objnum");
    return 1;
}

/* only called during startup, nothing can be dirty so no chance the cleaner can call it */
cObjnum lookup_first_objnum(void)
{
    datum key;

    key = dbm_firstkey(dbp);
    if (key.dptr == NULL)
        return INV_OBJNUM;
    if (key.dsize > 1 && *(char*)key.dptr == 0)
        return atoln(key.dptr + 1, key.dsize - 1);
    return lookup_next_objnum();
}

/* only called during startup, nothing can be dirty so no chance the cleaner can call it */
cObjnum lookup_next_objnum(void)
{
    datum key;

    key = dbm_nextkey(dbp);
    if (key.dptr == NULL)
        return NOT_AN_IDENT;
    if (key.dsize > 1 && *(char*)key.dptr == 0)
        return atoln(key.dptr + 1, key.dsize - 1);
    return lookup_next_objnum();
}

Int lookup_retrieve_name(Ident name, cObjnum *objnum)
{
    Int i = name % NAME_CACHE_SIZE;

    LOCK_LOOKUP("lookup_retrieve_name");
    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
        name_cache_hits++;
        *objnum = name_cache[i].objnum;
        UNLOCK_LOOKUP("lookup_retrieve_name");
        return 1;
    }

    name_cache_misses++;

    /* Get it from the database. */
    if (!get_name(name, objnum)) {
        UNLOCK_LOOKUP("lookup_retrieve_name");
        return 0;
    }

    /* Discard the old cache entry if it exists. */
    if (name_cache[i].name != NOT_AN_IDENT) {
        if (name_cache[i].dirty)
            store_name(name_cache[i].name, name_cache[i].objnum);
        ident_discard(name_cache[i].name);
    }

    /* Make a new cache entry. */
    name_cache[i].name = ident_dup(name);
    name_cache[i].objnum = *objnum;
    name_cache[i].dirty = 0;
    name_cache[i].on_disk = 1;

    UNLOCK_LOOKUP("lookup_retrieve_name");
    return 1;
}

Int lookup_store_name(Ident name, cObjnum objnum)
{
    Int i = name % NAME_CACHE_SIZE;

    LOCK_LOOKUP("lookup_store_name");

    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
        if (name_cache[i].objnum != objnum) {
            name_cache[i].objnum = objnum;
            name_cache[i].dirty = 1;
        }
        UNLOCK_LOOKUP("lookup_store_name");
        return 1;
    }

    /* Discard the old cache entry if it exists. */
    if (name_cache[i].name != NOT_AN_IDENT) {
        if (name_cache[i].dirty)
            store_name(name_cache[i].name, name_cache[i].objnum);
        ident_discard(name_cache[i].name);
    }

    /* Make a new cache entry. */
    name_cache[i].name = ident_dup(name);
    name_cache[i].objnum = objnum;
    name_cache[i].dirty = 1;
    name_cache[i].on_disk = 0;

    UNLOCK_LOOKUP("lookup_store_name");
    return 1;
}

Int lookup_remove_name(Ident name)
{
    datum key;
    Int i = name % NAME_CACHE_SIZE;

    LOCK_LOOKUP("lookup_remove_name");
    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
        /* Delete it from the cache.  If it's not on disk, then we're done. */
        /*write_err("##lookup_remove_name %d %s", name_cache[i].name, ident_name(name_cache[i].name));*/
        ident_discard(name_cache[i].name);
        name_cache[i].name = NOT_AN_IDENT;
        if (!name_cache[i].on_disk) {
            UNLOCK_LOOKUP("lookup_remove_name");
            return 1;
        }
    }

    /* Remove the key from the database. */
    key = name_key(name);
    if (dbm_delete(dbp, key)) {
        UNLOCK_LOOKUP("lookup_remove_name");
        return 0;
    }

    UNLOCK_LOOKUP("lookup_remove_name");
    return 1;
}

static datum objnum_key(cObjnum objnum, Number_buf nbuf)
{
    char *s;
    datum key;

    /* Set up a key for a objnum.  The first byte will be 0, distinguishing it
     * from a string. */
    s = long_to_ascii(objnum, nbuf);
    *--s = 0;
    key.dptr = s;
    key.dsize = strlen(s + 1) + 2;
    return key;
}

static datum offset_size_value(off_t offset, Int size, Number_buf nbuf)
{
    char *s;
    Number_buf tmp_buf;
    datum value;

    /* Set up a value for the offset and size. */
#if _FILE_OFFSET_BITS == 32
    s = long_to_ascii(offset, tmp_buf);
#else
    s = long_long_to_ascii(offset, tmp_buf);
#endif
    strcpy(nbuf, s);
    strcat(nbuf, ";");
    s = long_to_ascii(size, tmp_buf);
    strcat(nbuf, s);
    value.dptr = nbuf;
    value.dsize = strlen(nbuf) + 1;
    return value;
}

static void parse_offset_size_value(datum value, off_t *offset, Int *size)
{
    char *p;

#if _FILE_OFFSET_BITS == 32
    *offset = atol(value.dptr);
#else
    *offset = atoll(value.dptr);
#endif
    p = strchr(value.dptr, ';');
    *size = atol(p + 1);
}

static datum name_key(Ident name)
{
    datum key;

    int size;
    char* name_str = ident_name_size(name, &size);

    /* Set up a key for the name.  Include the 0 byte at the end. */
    key.dptr = name_str;
    key.dsize = size + 1;

    return key;
}

static datum objnum_value(cObjnum objnum, Number_buf nbuf)
{
    char *s;
    datum value;

    s = long_to_ascii(objnum, nbuf);
    value.dptr = s;
    value.dsize = strlen(s) + 1;
    return value;
}

static void sync_name_cache(void)
{
    Int i;

    write_err ("Syncing lookup name cache...");

    for (i = 0; i < NAME_CACHE_SIZE; i++) {
        if (name_cache[i].name != NOT_AN_IDENT && name_cache[i].dirty) {
            store_name(name_cache[i].name, name_cache[i].objnum);
            name_cache[i].dirty = 0;
            name_cache[i].on_disk = 1;
        }
    }
}

static Int store_name(Ident name, cObjnum objnum)
{
    datum key, value;
    Number_buf nbuf;

    /* Set up the value structure. */
    value = objnum_value(objnum, nbuf);

    key = name_key(name);
    if (dbm_store(dbp, key, value, DBM_REPLACE)) {
        write_err("ERROR: Failed to store key %s.", name);
        return 0;
    }

    return 1;
}

static Int get_name(Ident name, cObjnum *objnum)
{
    datum key, value;

    /* Get the key from the database. */
    key = name_key(name);
    value = dbm_fetch(dbp, key);
    if (!value.dptr)
        return 0;

    *objnum = atol(value.dptr);
    return 1;
}

