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
#include "cdc_db.h"
#include "util.h"

#include DBM_H_FILE

#ifdef S_IRUSR
#define READ_WRITE		(S_IRUSR | S_IWUSR)
#define READ_WRITE_EXECUTE	(S_IRUSR | S_IWUSR | S_IXUSR)
#else
#define READ_WRITE 0600
#define READ_WRITE_EXECUTE 0700
#endif

INTERNAL datum objnum_key(Long objnum, Number_buf nbuf);
INTERNAL datum name_key(Long name);
INTERNAL datum offset_size_value(off_t offset, Int size, Number_buf nbuf);
INTERNAL void parse_offset_size_value(datum value, off_t *offset, Int *size);
INTERNAL datum objnum_value(Long objnum, Number_buf nbuf);
INTERNAL void sync_name_cache(void);
INTERNAL Int store_name(Long name, Long objnum);
INTERNAL Int get_name(Long name, Long *objnum);

INTERNAL DBM *dbp;

struct name_cache_entry {
    Long name;
    Long objnum;
    char dirty;
    char on_disk;
} name_cache[NAME_CACHE_SIZE + 1];

void lookup_open(char *name, Int cnew) {
    Int i;

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

    /* Only way to do this with ndbm is close and re-open. */
    sync_name_cache();
    dbm_close(dbp);
    dbp = dbm_open(buf, O_RDWR | O_CREAT | O_BINARY, READ_WRITE);
    if (!dbp)
	panic("Cannot reopen dbm database file.");
}

Int lookup_retrieve_objnum(Long objnum, off_t *offset, Int *size)
{
    datum key, value;
    Number_buf nbuf;

    /* Get the value for objnum from the database. */
    key = objnum_key(objnum, nbuf);
    value = dbm_fetch(dbp, key);
    if (!value.dptr)
	return 0;

    parse_offset_size_value(value, offset, size);
    return 1;
}

Int lookup_store_objnum(Long objnum, off_t offset, Int size)
{
    datum key, value;
    Number_buf nbuf1, nbuf2;

    key = objnum_key(objnum, nbuf1);
    value = offset_size_value(offset, size, nbuf2);
    if (dbm_store(dbp, key, value, DBM_REPLACE)) {
	write_err("ERROR: Failed to store key %l.", objnum);
	return 0;
    }

    return 1;
}

Int lookup_remove_objnum(Long objnum)
{
    datum key;
    Number_buf nbuf;

    /* Remove the key from the database. */
    key = objnum_key(objnum, nbuf);
    if (dbm_delete(dbp, key)) {
	write_err("ERROR: Failed to delete key %l.", objnum);
	return 0;
    }
    return 1;
}

Long lookup_first_objnum(void)
{
    datum key;

    key = dbm_firstkey(dbp);
    if (key.dptr == NULL)
	return INV_OBJNUM;
    if (key.dsize > 1 && *key.dptr == 0)
	return atoln(key.dptr + 1, key.dsize - 1);
    return lookup_next_objnum();
}

Long lookup_next_objnum(void)
{
    datum key;

    key = dbm_nextkey(dbp);
    if (key.dptr == NULL)
	return NOT_AN_IDENT;
    if (key.dsize > 1 && *key.dptr == 0)
	return atoln(key.dptr + 1, key.dsize - 1);
    return lookup_next_objnum();
}

Int lookup_retrieve_name(Long name, Long *objnum)
{
    Int i = name % NAME_CACHE_SIZE;

    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
	*objnum = name_cache[i].objnum;
	return 1;
    }

    /* Get it from the database. */
    if (!get_name(name, objnum))
	return 0;

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

    return 1;
}

Int lookup_store_name(Long name, Long objnum)
{
    Int i = name % NAME_CACHE_SIZE;

    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
	if (name_cache[i].objnum != objnum) {
	    name_cache[i].objnum = objnum;
	    name_cache[i].dirty = 1;
	}
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

    return 1;
}

Int lookup_remove_name(Long name)
{
    datum key;
    Int i = name % NAME_CACHE_SIZE;

    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
	/* Delete it from the cache.  If it's not on disk, then we're done. */
	/*write_err("##lookup_remove_name %d %s", name_cache[i].name, ident_name(name_cache[i].name));*/
	ident_discard(name_cache[i].name);
	name_cache[i].name = NOT_AN_IDENT;
	if (!name_cache[i].on_disk)
	    return 1;
    }

    /* Remove the key from the database. */
    key = name_key(name);
    if (dbm_delete(dbp, key))
	return 0;
    return 1;
}

Long lookup_first_name(void)
{
    datum key;

    sync_name_cache();
    key = dbm_firstkey(dbp);
    if (key.dptr == NULL)
	return NOT_AN_IDENT;
    if (key.dsize == 1 || *key.dptr != 0)
	return ident_get(key.dptr);
    return lookup_next_name();
}

Long lookup_next_name(void)
{
    datum key;

    key = dbm_nextkey(dbp);
    if (key.dptr == NULL)
	return NOT_AN_IDENT;
    if (key.dsize == 1 || *key.dptr != 0)
	return ident_get(key.dptr);
    return lookup_next_name();
}

INTERNAL datum objnum_key(Long objnum, Number_buf nbuf)
{
    char *s;
    datum key;

    /* Set up a key for a objnum.  The first byte will be 0, distinguishing it
     * from a string. */
    s = long_to_ascii(objnum, nbuf);
#if DISABLED
    *--s = 0;
    key.dptr = s;
    key.dsize = strlen(s + 1) + 2;
#else
    key.dptr = s-1;
    key.dsize = strlen(s) + 2;
#endif
    return key;
}

INTERNAL datum offset_size_value(off_t offset, Int size, Number_buf nbuf)
{
    char *s;
    Number_buf tmp_buf;
    datum value;

    /* Set up a value for the offset and size. */
    s = long_to_ascii(offset, tmp_buf);
    nbuf[0] = 0;
    strcpy(nbuf, s);
    strcat(nbuf, ";");
    s = long_to_ascii(size, tmp_buf);
    strcat(nbuf, s);
    value.dptr = nbuf;
    value.dsize = strlen(nbuf) + 1;
    return value;
}

INTERNAL void parse_offset_size_value(datum value, off_t *offset, Int *size)
{
    char *p;

    *offset = atol(value.dptr);
    p = strchr(value.dptr, ';');
    *size = atol(p + 1);
}

INTERNAL datum name_key(Long name)
{
    datum key;

    /* Set up a key for the name.  Include the 0 byte at the end. */
    key.dptr = ident_name(name);
    key.dsize = strlen(key.dptr) + 1;
    return key;
}

INTERNAL datum objnum_value(Long objnum, Number_buf nbuf)
{
    char *s;
    datum value;

    s = long_to_ascii(objnum, nbuf);
    value.dptr = s;
    value.dsize = strlen(s) + 1;
    return value;
}

INTERNAL void sync_name_cache(void)
{
    Int i;

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

INTERNAL Int get_name(Long name, Long *objnum)
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

