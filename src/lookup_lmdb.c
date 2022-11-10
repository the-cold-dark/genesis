/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Interface to db index of object locations.
*/

#include "defs.h"

#include <sys/types.h>
#ifdef __UNIX__
#include <sys/file.h>
#endif
#include <sys/stat.h>
#include "third-party/lmdb/lmdb.h"
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
#define UNLOCK_LOOKUP(func) do { \
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

Int name_cache_hits = 0;
Int name_cache_misses = 0;

typedef struct _offset_size _offset_size;
struct _offset_size {
    off_t offset;
    Int   size;
};

static void objnum_keyvalue(cObjnum *objnum, MDB_val *key);
static void name_key(Ident name, MDB_val *key);
static void sync_name_cache(void);
static bool store_name(Ident name, cObjnum objnum);
static bool get_name(Ident name, cObjnum *objnum);

//static MDB_env *objnum_env;
static MDB_dbi objnum_dbi;
//static MDB_env *name_env;
static MDB_dbi name_dbi;
static MDB_cursor *name_cursor;

struct name_cache_entry {
    Ident   name;
    cObjnum objnum;
    char    dirty;
    char    on_disk;
} name_cache[NAME_CACHE_SIZE + 1];

void lookup_open(const char *name, bool cnew) {
    Int i;
    int ret;
    char * objnum_name,
         * name_name;

    objnum_name = (char*) malloc(strlen(name) + 8);
    objnum_name[0] = 0;
    strcat(objnum_name, name);
    strcat(objnum_name, ".objnum");

    name_name = (char*) malloc(strlen(name) + 6);
    name_name[0] = 0;
    strcat(name_name, name);
    strcat(name_name, ".name");

#ifdef USE_CLEANER_THREAD
    pthread_mutex_init(&lookup_mutex, NULL);
#endif

    if ((ret = db_create(&objnum_dbi, NULL, 0)) != 0) {
        fprintf(stderr, "db_create: %s\n", mdb_strerror(ret));
        exit(1);
    }

    if (cnew)
        ret = objnum_dbi->open(objnum_dbi, NULL, objnum_name, NULL, DB_BTREE,
                               DB_TRUNCATE | DB_CREATE, 0664);
    else
        ret = objnum_dbi->open(objnum_dbi, NULL, objnum_name, NULL, DB_BTREE,
                               DB_CREATE, 0664);

    if (ret != 0)
        fail_to_start("Cannot open objnum bdb database file.");

    if ((ret = db_create(&name_dbi, NULL, 0)) != 0) {
        fprintf(stderr, "db_create: %s\n", mdb_strerror(ret));
        exit(1);
    }

    if (cnew)
        ret = name_dbi->open(name_dbi, NULL, name_name, NULL, DB_BTREE,
                               DB_TRUNCATE | DB_CREATE, 0664);
    else
        ret = name_dbi->open(name_dbi, NULL, name_name, NULL, DB_BTREE,
                               DB_CREATE, 0664);

    if (ret != 0)
        fail_to_start("Cannot open name bdb database file.");

    for (i = 0; i < NAME_CACHE_SIZE; i++)
        name_cache[i].name = NOT_AN_IDENT;

    free(objnum_name);
    free(name_name);
}

void lookup_close(void) {
    int ret;

    sync_name_cache();

    mdb_dbi_close(NULL, objnum_dbi);
    mdb_dbi_close(NULL, name_dbi);
}

void lookup_sync(void) {
    char buf[255];
    int ret1, ret2;

    sprintf(buf, "%s/index", c_dir_binary);

    LOCK_LOOKUP("lookup_sync");

    sync_name_cache();
    ret1 = objnum_dbi->sync(objnum_dbi, 0);
    ret2 = name_dbi->sync(name_dbi, 0);

    UNLOCK_LOOKUP("lookup_sync");

    if ((ret1 != 0) || (ret2 != 0))
        panic("Cannot sync index database file.");
}

bool lookup_retrieve_objnum(cObjnum objnum, off_t *offset, Int *size)
{
    MDB_val key, value;

    LOCK_LOOKUP("lookup_retrieve_objnum");

    /* Get the value for objnum from the database. */
    objnum_keyvalue(&objnum, &key);
    memset(&value, 0, sizeof(value));
    if (mdb_get(NULL, objnum_dbi, &key, &value) != 0) {
        UNLOCK_LOOKUP("lookup_retrieve_objnum");
        return false;
    }

    _offset_size *os = (_offset_size*)value.mv_data;
    *offset = os->offset;
    *size = os->size;

    UNLOCK_LOOKUP("lookup_retrieve_objnum");
    return true;
}

bool lookup_store_objnum(cObjnum objnum, off_t offset, Int size)
{
    MDB_val key, value;
    _offset_size os;
    int ret;

    LOCK_LOOKUP("lookup_store_objnum");
    objnum_keyvalue(&objnum, &key);

    memset(&value, 0, sizeof(value));

    os.offset = offset;
    os.size = size;

    value.mv_data = &os;
    value.mv_size = sizeof(os);

    if ((ret = mdb_put(NULL, objnum_dbi, &key, &value, 0)) != 0) {
        write_err("ERROR: Failed to store key %l.", objnum);
        UNLOCK_LOOKUP("lookup_store_objnum");
        return false;
    }

    UNLOCK_LOOKUP("lookup_store_objnum");
    return true;
}

bool lookup_remove_objnum(cObjnum objnum)
{
    MDB_val key;
    int ret;

    LOCK_LOOKUP("lookup_remove_objnum");
    /* Remove the key from the database. */
    objnum_keyvalue(&objnum, &key);
    if ((ret = mdb_del(NULL, objnum_dbi, &key, NULL)) != 0) {
        write_err("ERROR: Failed to delete key %l.", objnum);
        UNLOCK_LOOKUP("lookup_remove_objnum");
        return false;
    }
    UNLOCK_LOOKUP("lookup_remove_objnum");
    return true;
}

/* only called during startup, nothing can be dirty so no chance the cleaner can call it */
cObjnum lookup_first_objnum(void)
{
    MDB_val key, value;
    int ret;

    ret = mdb_cursor_open(NULL, objnum_dbi, &name_cursor);

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    ret = mdb_cursor_get(name_cursor, &key, &value, MDB_FIRST);
    if (ret != 0) {
        return INV_OBJNUM;
    }

    return *(cObjnum*)key.mv_data;
}

/* only called during startup, nothing can be dirty so no chance the cleaner can call it */
cObjnum lookup_next_objnum(void)
{
    MDB_val key, value;
    int ret;

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    ret = mdb_cursor_get(name_cursor, &key, &value, MDB_NEXT);
    if (ret != 0) {
        mdb_cursor_close(name_cursor);
        return NOT_AN_IDENT;
    }

    return *(cObjnum*)key.mv_data;
}

bool lookup_retrieve_name(Ident name, cObjnum *objnum)
{
    Int i = name % NAME_CACHE_SIZE;

    LOCK_LOOKUP("lookup_retrieve_name");
    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
        name_cache_hits++;
        *objnum = name_cache[i].objnum;
        UNLOCK_LOOKUP("lookup_retrieve_name");
        return true;
    }

    name_cache_misses++;

    /* Get it from the database. */
    if (!get_name(name, objnum)) {
        UNLOCK_LOOKUP("lookup_retrieve_name");
        return false;
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
    return true;
}

bool lookup_store_name(Ident name, cObjnum objnum)
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
        return true;
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
    return true;
}

bool lookup_remove_name(Ident name)
{
    MDB_val key;
    Int i = name % NAME_CACHE_SIZE;
    int ret;

    LOCK_LOOKUP("lookup_remove_name");
    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
        /* Delete it from the cache.  If it's not on disk, then we're done. */
        /*write_err("##lookup_remove_name %d %s", name_cache[i].name, ident_name(name_cache[i].name));*/
        ident_discard(name_cache[i].name);
        name_cache[i].name = NOT_AN_IDENT;
        if (!name_cache[i].on_disk) {
            UNLOCK_LOOKUP("lookup_remove_name");
            return true;
        }
    }

    /* Remove the key from the database. */
    name_key(name, &key);
    if ((ret = mdb_del(NULL, name_dbi, &key, NULL)) != 0) {
        UNLOCK_LOOKUP("lookup_remove_name");
        return false;
    }

    UNLOCK_LOOKUP("lookup_remove_name");
    return true;
}

static void objnum_keyvalue(cObjnum *objnum, MDB_val *key)
{
    memset(key, 0, sizeof(*key));
    key->mv_data = objnum;
    key->mv_size = sizeof(cObjnum);
}

static void name_key(Ident name, MDB_val *key)
{
    int size;
    char* name_str = ident_name_size(name, &size);

    memset(key, 0, sizeof(*key));
    /* Set up a key for the name.  Include the 0 byte at the end. */
    key->mv_data = name_str;
    key->mv_size = size + 1;
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

static bool store_name(Ident name, cObjnum objnum)
{
    MDB_val key, value;
    int ret;

    /* Set up the value structure. */
    objnum_keyvalue(&objnum, &value);

    name_key(name, &key);
    if ((ret = mdb_put(NULL, name_dbi, &key, &value, 0)) != 0) {
        write_err("ERROR: Failed to store key '%s`: %s.", name, mdb_strerror(ret));
        return false;
    }

    return true;
}

static bool get_name(Ident name, cObjnum *objnum)
{
    MDB_val key, value;

    /* Get the key from the database. */
    name_key(name, &key);
    memset(&value, 0, sizeof(value));
    if (mdb_get(NULL, name_dbi, &key, &value) != 0) {
        return false;
    }

    memcpy(
        (unsigned char*)(objnum),
        (unsigned char*)value.mv_data,
        sizeof(cObjnum));

    return true;
}

