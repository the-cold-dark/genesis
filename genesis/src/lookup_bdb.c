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
#include "db.h"
#include <fcntl.h>
#include <string.h>

#ifdef USE_CLEANER_THREAD
pthread_mutex_t lookup_mutex;

#ifdef DEBUG_LOOKUP_LOCK
#define LOCK_LOOKUP(func) \
        write_err("%s: locking db", func); \
	pthread_mutex_lock(&lookup_mutex); \
	write_err("%s: locked db", func);
#define UNLOCK_LOOKUP(func) \
	pthread_mutex_unlock(&lookup_mutex); \
	write_err("%s: unlocked db", func);
#else
#define LOCK_LOOKUP(func) \
	pthread_mutex_lock(&lookup_mutex);
#define UNLOCK_LOOKUP(func) \
	pthread_mutex_unlock(&lookup_mutex);
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

INTERNAL void objnum_keyvalue(Long objnum, char *buf, DBT *key);
INTERNAL void name_key(Long name, DBT *key);
INTERNAL void offset_size_value(off_t offset, Int size,
                                _offset_size *os, DBT *value);
INTERNAL void parse_offset_size_value(DBT *value, off_t *offset, Int *size);
INTERNAL void sync_name_cache(void);
INTERNAL Int store_name(Long name, Long objnum);
INTERNAL Int get_name(Long name, Long *objnum);

INTERNAL DB *objnum_dbp;
INTERNAL DB *name_dbp;
INTERNAL DBC *dbc;

struct name_cache_entry {
    Long name;
    Long objnum;
    char dirty;
    char on_disk;
} name_cache[NAME_CACHE_SIZE + 1];

void lookup_open(char *name, Int cnew) {
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

    if ((ret = db_create(&objnum_dbp, NULL, 0)) != 0) {
        fprintf(stderr, "db_create: %s\n", db_strerror(ret));
        exit(1);
    }

    if ((ret = objnum_dbp->set_cachesize(objnum_dbp, 0, 
                                         75*1024*1024, 1)) != 0) {
        objnum_dbp->err(objnum_dbp, ret, "DB->set_cachesize: %s", name);
        exit(1);
    }

    if ((ret = objnum_dbp->set_pagesize(objnum_dbp, 4096)) != 0) {
        objnum_dbp->err(objnum_dbp, ret, "DB->set_pagesize: %s", name);
        exit(1);
    }

    objnum_dbp->set_errfile(objnum_dbp, stderr);

    if (cnew)
	ret = objnum_dbp->open(objnum_dbp, objnum_name, NULL, DB_BTREE, 
                               DB_TRUNCATE | DB_CREATE, 0664);
    else
	ret = objnum_dbp->open(objnum_dbp, objnum_name, NULL, DB_BTREE,
                               DB_CREATE, 0664);
    if (ret != 0)
	fail_to_start("Cannot open objnum bdb database file.");

    if ((ret = db_create(&name_dbp, NULL, 0)) != 0) {
        fprintf(stderr, "db_create: %s\n", db_strerror(ret));
        exit(1);
    }

    if ((ret = name_dbp->set_cachesize(name_dbp, 0,
                                         75*1024*1024, 1)) != 0) {
        name_dbp->err(name_dbp, ret, "DB->set_cachesize: %s", name);
        exit(1);
    }

    if ((ret = name_dbp->set_pagesize(name_dbp, 4096)) != 0) {
        name_dbp->err(name_dbp, ret, "DB->set_pagesize: %s", name);
        exit(1);
    }

    name_dbp->set_errfile(name_dbp, stderr);

    if (cnew)
        ret = name_dbp->open(name_dbp, name_name, NULL, DB_BTREE,
                               DB_TRUNCATE | DB_CREATE, 0664);
    else
        ret = name_dbp->open(name_dbp, name_name, NULL, DB_BTREE,
                               DB_CREATE, 0664);
    if (ret != 0)
        fail_to_start("Cannot open name bdb database file.");

    for (i = 0; i < NAME_CACHE_SIZE; i++)
	name_cache[i].name = NOT_AN_IDENT;
}

void lookup_close(void) {
    int ret;

    sync_name_cache();

    if ((ret = objnum_dbp->close(objnum_dbp, 0)) != 0) {
        objnum_dbp->err(objnum_dbp, ret, "objnum_db close");
        exit(1);
    }

    if ((ret = name_dbp->close(name_dbp, 0)) != 0) {
        name_dbp->err(name_dbp, ret, "name_db close");
        exit(1);
    }
}

void lookup_sync(void) {
    char buf[255];
    int ret1, ret2;

    sprintf(buf, "%s/index", c_dir_binary);

    LOCK_LOOKUP("lookup_sync")

    sync_name_cache();
    ret1 = objnum_dbp->sync(objnum_dbp, 0);
    ret2 = name_dbp->sync(name_dbp, 0);

    UNLOCK_LOOKUP("lookup_sync")

    if ((ret1 != 0) || (ret2 != 0))
	panic("Cannot sync index database file.");
}

Int lookup_retrieve_objnum(Long objnum, off_t *offset, Int *size)
{
    DBT key, value;
    int ret;
    char buf[SIZEOF_LONG];

    LOCK_LOOKUP("lookup_retrieve_objnum")

    /* Get the value for objnum from the database. */
    objnum_keyvalue(objnum, buf, &key);
    memset(&value, 0, sizeof(value));
    if ((ret = objnum_dbp->get(objnum_dbp, NULL, &key, &value, 0)) != 0)
    {
	UNLOCK_LOOKUP("lookup_retrieve_objnum")
	return 0;
    }

    parse_offset_size_value(&value, offset, size);
    UNLOCK_LOOKUP("lookup_retrieve_objnum")
    return 1;
}

Int lookup_store_objnum(Long objnum, off_t offset, Int size)
{
    DBT key, value;
    _offset_size os;
    char buf[SIZEOF_LONG];
    int ret;

    LOCK_LOOKUP("lookup_store_objnum")
    objnum_keyvalue(objnum, buf, &key);
    offset_size_value(offset, size, &os, &value);
    if ((ret =  objnum_dbp->put(objnum_dbp, NULL, &key, &value, 0)) != 0) {
	write_err("ERROR: Failed to store key %l.", objnum);
        objnum_dbp->err(objnum_dbp, ret, "lookup_store_objnum");
        UNLOCK_LOOKUP("lookup_store_objnum")
	return 0;
    }

    UNLOCK_LOOKUP("lookup_store_objnum")
    return 1;
}

Int lookup_remove_objnum(Long objnum)
{
    DBT key;
    char buf[SIZEOF_LONG];
    int ret;

    LOCK_LOOKUP("lookup_remove_objnum")
    /* Remove the key from the database. */
    objnum_keyvalue(objnum, buf, &key);
    if ((ret = objnum_dbp->del(objnum_dbp, NULL, &key, 0)) != 0) {
	write_err("ERROR: Failed to delete key %l.", objnum);
        UNLOCK_LOOKUP("lookup_remove_objnum")
	return 0;
    }
    UNLOCK_LOOKUP("lookup_remove_objnum")
    return 1;
}

/* only called during startup, nothing can be dirty so no chance the cleaner can call it */
Long lookup_first_objnum(void)
{
    DBT key, value;
    int ret;

    ret = objnum_dbp->cursor(objnum_dbp, NULL, &dbc, 0);

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    ret = dbc->c_get(dbc, &key, &value, DB_FIRST);
    if (ret != 0)
	return INV_OBJNUM;

    return (*(Long*)key.data);
}

/* only called during startup, nothing can be dirty so no chance the cleaner can call it */
Long lookup_next_objnum(void)
{
    DBT key, value;
    int ret;

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    ret = dbc->c_get(dbc, &key, &value, DB_NEXT);
    if (ret != 0) {
        dbc->c_close(dbc);
        return NOT_AN_IDENT;
    }

    return (*(Long*)key.data);
}

Int lookup_retrieve_name(Long name, Long *objnum)
{
    Int i = name % NAME_CACHE_SIZE;

    LOCK_LOOKUP("lookup_retrieve_name")
    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
        name_cache_hits++;
	*objnum = name_cache[i].objnum;
        UNLOCK_LOOKUP("lookup_retrieve_name")
	return 1;
    }

    name_cache_misses++;

    /* Get it from the database. */
    if (!get_name(name, objnum)) {
        UNLOCK_LOOKUP("lookup_retrieve_name")
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

    UNLOCK_LOOKUP("lookup_retrieve_name")
    return 1;
}

Int lookup_store_name(Long name, Long objnum)
{
    Int i = name % NAME_CACHE_SIZE;

    LOCK_LOOKUP("lookup_store_name")

    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
	if (name_cache[i].objnum != objnum) {
	    name_cache[i].objnum = objnum;
	    name_cache[i].dirty = 1;
	}
        UNLOCK_LOOKUP("lookup_store_name")
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

    UNLOCK_LOOKUP("lookup_store_name")
    return 1;
}

Int lookup_remove_name(Long name)
{
    DBT key;
    Int i = name % NAME_CACHE_SIZE;
    int ret;

    LOCK_LOOKUP("lookup_remove_name")
    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
	/* Delete it from the cache.  If it's not on disk, then we're done. */
	/*write_err("##lookup_remove_name %d %s", name_cache[i].name, ident_name(name_cache[i].name));*/
	ident_discard(name_cache[i].name);
	name_cache[i].name = NOT_AN_IDENT;
	if (!name_cache[i].on_disk) {
            UNLOCK_LOOKUP("lookup_remove_name")
	    return 1;
	}
    }

    /* Remove the key from the database. */
    name_key(name, &key);
    if ((ret = name_dbp->del(name_dbp, NULL, &key, 0)) != 0) {
        UNLOCK_LOOKUP("lookup_remove_name")
	return 0;
    }

    UNLOCK_LOOKUP("lookup_remove_name")
    return 1;
}

INTERNAL void objnum_keyvalue(Long objnum, char *buf, DBT *key)
{
    memset(key, 0, sizeof(*key));
    memcpy(buf, (uChar*)(&objnum), sizeof(objnum));
    key->data = buf;
    key->size = sizeof(objnum);
}

INTERNAL void offset_size_value(off_t offset, Int size,
                                _offset_size *os, DBT *value)
{
    memset(os, 0, sizeof(*os));
    memset(value, 0, sizeof(*value));
    /* Set up a value for the offset and size. */
    os->offset = offset;
    os->size = size;
    value->data = os;
    value->size = sizeof(*os);
}

INTERNAL void parse_offset_size_value(DBT *value, off_t *offset, Int *size)
{
    _offset_size *os = (_offset_size*)value->data;

    *offset = os->offset;
    *size = os->size;
}

INTERNAL void name_key(Long name, DBT *key)
{
    int size;
    char* name_str = ident_name_size(name, &size);

    memset(key, 0, sizeof(*key));
    /* Set up a key for the name.  Include the 0 byte at the end. */
    key->data = name_str;
    key->size = size + 1;
}

INTERNAL void sync_name_cache(void)
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

INTERNAL Int store_name(Long name, Long objnum)
{
    DBT key, value;
    char buf[SIZEOF_LONG];
    int ret;

    /* Set up the value structure. */
    objnum_keyvalue(objnum, buf, &value);

    name_key(name, &key);
    if ((ret = name_dbp->put(name_dbp, NULL, &key, &value, 0)) != 0) {
	write_err("ERROR: Failed to store key %s.", name);
        name_dbp->err(name_dbp, ret, "store_name: %s", "");
	return 0;
    }

    return 1;
}

INTERNAL Int get_name(Long name, Long *objnum)
{
    DBT key, value;
    int ret;

    /* Get the key from the database. */
    name_key(name, &key);
    memset(&value, 0, sizeof(value));
    if ((ret = name_dbp->get(name_dbp, NULL, &key, &value, 0)) != 0) {
	return 0;
    }

    memcpy((uChar*)(objnum), (uChar*)value.data, sizeof(objnum));

    return 1;
}

