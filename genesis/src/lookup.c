/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: lookup.c
// ---
// Interface to dbm index of object locations.
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <ndbm.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "defs.h"
#include "lookup.h"
#include "log.h"
#include "ident.h"
#include "util.h"

#ifdef S_IRUSR
#define READ_WRITE		(S_IRUSR | S_IWUSR)
#define READ_WRITE_EXECUTE	(S_IRUSR | S_IWUSR | S_IXUSR)
#else
#define READ_WRITE 0600
#define READ_WRITE_EXECUTE 0700
#endif

#define NAME_CACHE_SIZE 503

static datum dbref_key(long dbref, Number_buf nbuf);
static datum name_key(long name);
static datum offset_size_value(off_t offset, int size, Number_buf nbuf);
static void parse_offset_size_value(datum value, off_t *offset, int *size);
static datum dbref_value(long dbref, Number_buf nbuf);
static void sync_name_cache(void);
static int store_name(long name, long dbref);
static int get_name(long name, long *dbref);

static DBM *dbp;

struct name_cache_entry {
    long name;
    long dbref;
    char dirty;
    char on_disk;
} name_cache[NAME_CACHE_SIZE];

void lookup_open(char *name, int cnew) {
    int i;

    if (cnew)
	dbp = dbm_open(name, O_TRUNC | O_RDWR | O_CREAT, READ_WRITE);
    else
	dbp = dbm_open(name, O_RDWR, READ_WRITE);
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
    dbp = dbm_open(buf, O_RDWR | O_CREAT, READ_WRITE);
    if (!dbp)
	panic("Cannot reopen dbm database file.");
}

int lookup_retrieve_dbref(long dbref, off_t *offset, int *size)
{
    datum key, value;
    Number_buf nbuf;

    /* Get the value for dbref from the database. */
    key = dbref_key(dbref, nbuf);
    value = dbm_fetch(dbp, key);
    if (!value.dptr)
	return 0;

    parse_offset_size_value(value, offset, size);
    return 1;
}

int lookup_store_dbref(long dbref, off_t offset, int size)
{
    datum key, value;
    Number_buf nbuf1, nbuf2;

    key = dbref_key(dbref, nbuf1);
    value = offset_size_value(offset, size, nbuf2);
    if (dbm_store(dbp, key, value, DBM_REPLACE)) {
	write_err("ERROR: Failed to store key %l.", dbref);
	return 0;
    }

    return 1;
}

int lookup_remove_dbref(long dbref)
{
    datum key;
    Number_buf nbuf;

    /* Remove the key from the database. */
    key = dbref_key(dbref, nbuf);
    if (dbm_delete(dbp, key)) {
	write_err("ERROR: Failed to delete key %l.", dbref);
	return 0;
    }
    return 1;
}

long lookup_first_dbref(void)
{
    datum key;

    key = dbm_firstkey(dbp);
    if (key.dptr == NULL)
	return NOT_AN_IDENT;
    if (key.dsize > 1 && *key.dptr == 0)
	return atoln(key.dptr + 1, key.dsize - 1);
    return lookup_next_dbref();
}

long lookup_next_dbref(void)
{
    datum key;

    key = dbm_nextkey(dbp);
    if (key.dptr == NULL)
	return NOT_AN_IDENT;
    if (key.dsize > 1 && *key.dptr == 0)
	return atoln(key.dptr + 1, key.dsize - 1);
    return lookup_next_dbref();
}

int lookup_retrieve_name(long name, long *dbref)
{
    int i = name % NAME_CACHE_SIZE;

    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
	*dbref = name_cache[i].dbref;
	return 1;
    }

    /* Get it from the database. */
    if (!get_name(name, dbref))
	return 0;

    /* Discard the old cache entry if it exists. */
    if (name_cache[i].name != NOT_AN_IDENT) {
	if (name_cache[i].dirty)
	    store_name(name_cache[i].name, name_cache[i].dbref);
	ident_discard(name_cache[i].name);
    }

    /* Make a new cache entry. */
    name_cache[i].name = ident_dup(name);
    name_cache[i].dbref = *dbref;
    name_cache[i].dirty = 0;
    name_cache[i].on_disk = 1;

    return 1;
}

int lookup_store_name(long name, long dbref)
{
    int i = name % NAME_CACHE_SIZE;

    /* See if it's in the cache. */
    if (name_cache[i].name == name) {
	if (name_cache[i].dbref != dbref) {
	    name_cache[i].dbref = dbref;
	    name_cache[i].dirty = 1;
	}
	return 1;
    }

    /* Discard the old cache entry if it exists. */
    if (name_cache[i].name != NOT_AN_IDENT) {
	if (name_cache[i].dirty)
	    store_name(name_cache[i].name, name_cache[i].dbref);
	ident_discard(name_cache[i].name);
    }

    /* Make a new cache entry. */
    name_cache[i].name = ident_dup(name);
    name_cache[i].dbref = dbref;
    name_cache[i].dirty = 1;
    name_cache[i].on_disk = 0;

    return 1;
}

int lookup_remove_name(long name)
{
    datum key;
    int i = name % NAME_CACHE_SIZE;

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

long lookup_first_name(void)
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

long lookup_next_name(void)
{
    datum key;

    key = dbm_nextkey(dbp);
    if (key.dptr == NULL)
	return NOT_AN_IDENT;
    if (key.dsize == 1 || *key.dptr != 0)
	return ident_get(key.dptr);
    return lookup_next_name();
}

static datum dbref_key(long dbref, Number_buf nbuf)
{
    char *s;
    datum key;

    /* Set up a key for a dbref.  The first byte will be 0, distinguishing it
     * from a string. */
    s = long_to_ascii(dbref, nbuf);
    *--s = 0;
    key.dptr = s;
    key.dsize = strlen(s + 1) + 2;
    return key;
}

static datum offset_size_value(off_t offset, int size, Number_buf nbuf)
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

static void parse_offset_size_value(datum value, off_t *offset, int *size)
{
    char *p;

    *offset = atol(value.dptr);
    p = strchr(value.dptr, ';');
    *size = atol(p + 1);
}

static datum name_key(long name)
{
    datum key;

    /* Set up a key for the name.  Include the 0 byte at the end. */
    key.dptr = ident_name(name);
    key.dsize = strlen(key.dptr) + 1;
    return key;
}

static datum dbref_value(long dbref, Number_buf nbuf)
{
    char *s;
    datum value;

    s = long_to_ascii(dbref, nbuf);
    value.dptr = s;
    value.dsize = strlen(s) + 1;
    return value;
}

static void sync_name_cache(void)
{
    int i;

    for (i = 0; i < NAME_CACHE_SIZE; i++) {
	if (name_cache[i].name != NOT_AN_IDENT && name_cache[i].dirty) {
	    store_name(name_cache[i].name, name_cache[i].dbref);
	    name_cache[i].dirty = 0;
	    name_cache[i].on_disk = 1;
	}
    }
}

static int store_name(long name, long dbref)
{
    datum key, value;
    Number_buf nbuf;

    /* Set up the value structure. */
    value = dbref_value(dbref, nbuf);

    key = name_key(name);
    if (dbm_store(dbp, key, value, DBM_REPLACE)) {
	write_err("ERROR: Failed to store key %s.", name);
	return 0;
    }

    return 1;
}

static int get_name(long name, long *dbref)
{
    datum key, value;

    /* Get the key from the database. */
    key = name_key(name);
    value = dbm_fetch(dbp, key);
    if (!value.dptr)
	return 0;

    *dbref = atol(value.dptr);
    return 1;
}

