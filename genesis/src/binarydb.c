/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: binarydb.c
// ---
// Object storage routines.
//
// The block allocation algorithm in this code is due to Marcus J. Ranum.
*/

#include "config.h"
#include "defs.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "binarydb.h"
#include "lookup.h"
#include "object.h"
#include "cache.h"
#include "log.h"
#include "util.h"
#include "dbpack.h"
#include "memory.h"
#include "ident.h"
#include "moddef.h"

#define NEEDED(n, b)		(((n) % (b)) ? (n) / (b) + 1 : (n) / (b))
#define ROUND_UP(a, m)		(((a) - 1) + (m) - (((a) - 1) % (m)))

#define	BLOCK_SIZE		256		/* Default block size */
#define	DB_BITBLOCK		512		/* Bitmap growth in blocks */
#define	LOGICAL_BLOCK(off)	((off) / BLOCK_SIZE)
#define	BLOCK_OFFSET(block)	((block) * BLOCK_SIZE)

#ifdef S_IRUSR
#define READ_WRITE		(S_IRUSR | S_IWUSR)
#define READ_WRITE_EXECUTE	(S_IRUSR | S_IWUSR | S_IXUSR)
#else
#define READ_WRITE 0600
#define READ_WRITE_EXECUTE 0700
#endif

static void db_mark(off_t start, int size);
static void db_unmark(off_t start, int size);
static void grow_bitmap(int new_blocks);
static int db_alloc(int size);
static void db_is_clean(void);
static void db_is_dirty(void);

static int last_free = 0;	/* Last known or suspected free block */

static FILE *database_file = NULL;

static char *bitmap = NULL;
static int bitmap_blocks = 0;

char c_clean_file[255];

static int db_clean;

extern long cur_search, db_top;

/* this isn't the most graceful way, but *shrug* */
#define FAIL(__s)        { fprintf(errfile, __s, c_dir_binary); exit(1); }
#define DBFILE(__b, __f) (sprintf(__b, "%s/%s", c_dir_binary, __f))

#define open_db_directory() { \
        if (stat(c_dir_binary, &statbuf) == F_FAILURE) { \
            if (mkdir(c_dir_binary, READ_WRITE_EXECUTE) == F_FAILURE) \
                FAIL("Cannot create binary directory \"%s\".\n"); \
        } else if (!S_ISDIR(statbuf.st_mode)) { \
            if (unlink(c_dir_binary) == F_FAILURE) \
                FAIL("Cannot delete file \"%s\".\n"); \
            if (mkdir(c_dir_binary, READ_WRITE_EXECUTE) == F_FAILURE) \
                FAIL("Cannot create directory \"%s\".\n"); \
        } \
    }
    
#define init_bitmaps() { \
        if (stat(fdb_objects, &statbuf) < 0) \
            FAIL("Cannot stat database file \"%s/objects\".\n"); \
        bitmap_blocks = ROUND_UP(LOGICAL_BLOCK(statbuf.st_size) + \
                        DB_BITBLOCK, 8); \
        bitmap = EMALLOC(char, (bitmap_blocks / 8)+1); \
        memset(bitmap, 0, (bitmap_blocks / 8)+1); \
    }

#define sync_index() { \
        objnum = lookup_first_objnum(); \
        while (objnum != NOT_AN_IDENT) { \
            if (!lookup_retrieve_objnum(objnum, &offset, &size)) \
                FAIL("Database index (\"%s/index\") is inconsistent.\n"); \
            if (objnum >= db_top) \
                db_top = objnum + 1; \
            db_mark(LOGICAL_BLOCK(offset), size); \
            objnum = lookup_next_objnum(); \
        } \
    }

#define open_db_objects(__p) { \
        database_file = fopen(fdb_objects, __p); \
        if (!database_file) \
            FAIL("Cannot open object database file \"%s/objects\".\n"); \
    }

void init_binary_db(void) {
    struct stat   statbuf;
    FILE        * fp;
    char          buf[LINE],
                  v_major[WORD],
                  v_minor[WORD],
                  v_patch[WORD],
                  magicmod[LINE],
                  fdb_clean[LINE],
                  fdb_objects[LINE],
                  fdb_index[LINE];
    off_t         offset;
    int           size,
                  outdated = 1;
    long          objnum;

    sprintf(c_clean_file, "%s/clean", c_dir_binary);
    DBFILE(fdb_clean,   "clean");
    DBFILE(fdb_objects, "objects");
    DBFILE(fdb_index,   "index");

    if (stat(c_dir_binary, &statbuf) == F_FAILURE) {
        FAIL("Cannot find binary directory \"%s\".\n");
    } else if (!S_ISDIR(statbuf.st_mode)) {
        FAIL("Binary db \"%s\" is not a directory.\n");
    }

    fp = fopen(fdb_clean, "rb");
    if (fp) {
        if (fgets(v_major, WORD, fp) && atoi(v_major)==VERSION_MAJOR) {
          if (fgets(v_minor, WORD, fp) && atoi(v_minor)==VERSION_MINOR) {
            if (fgets(v_patch, WORD, fp) && atoi(v_patch)==VERSION_PATCH) {
              if (fgets(magicmod, LINE, fp)&&atol(magicmod)==MAGIC_MODNUMBER) {
                  fgets(buf, LINE, fp);
                  cur_search = atoi(buf);
                  fgets(buf, LINE, fp);
                  /* eat the newline */
                  if (buf[strlen(buf) - 1] == (char) 10)
                      buf[strlen(buf) - 1] = (char) NULL;
  
                  if (!strcmp(buf, SYSTEM_TYPE))
                      outdated = 0;
              }
            }
          }
        }
        fclose(fp);
    } else {
        FAIL("Binary database (\"%s\") is corrupted, aborting...\n");
    }

    if (outdated) {
        fprintf(errfile, "Binary database \"%s\" is incompatable.\n\
It was compiled on %s with coldcc %d.%d-%d and module code %li\n\
This system is %s with coldcc %d.%d-%d and module code %li\n\n",
        c_dir_binary, buf, atoi(v_major), atoi(v_minor), atoi(v_patch),
        atol(magicmod), SYSTEM_TYPE, VERSION_MAJOR, VERSION_MINOR,
        VERSION_PATCH, (long) MAGIC_MODNUMBER);
        FAIL("Unable to load database \"%s\".\n");
    }

    open_db_objects("rb+");
    lookup_open(fdb_index, 0);
    init_bitmaps();
    sync_index();

    db_clean = 1;
}

void init_new_db(void) {
    struct stat   statbuf;
    char          fdb_objects[LINE],
                  fdb_index[LINE];
    off_t         offset;
    int           size;
    long          objnum;

    sprintf(c_clean_file, "%s/clean", c_dir_binary);
    DBFILE(fdb_objects, "objects");
    DBFILE(fdb_index,   "index");

    open_db_directory();
    open_db_objects("w+");
    lookup_open(fdb_index, 1);
    init_bitmaps();
    sync_index();
    db_is_clean();
}

int init_db(int force_textdump) {
    struct stat   statbuf;
    FILE        * fp;
    char          buf[LINE],
                  fdb_clean[LINE],
                  fdb_objects[LINE],
                  fdb_index[LINE];
    off_t         offset;
    int           cnew = 1,
                  size;
    long          objnum;

    sprintf(c_clean_file, "%s/clean", c_dir_binary);
    DBFILE(fdb_clean,   "clean");
    DBFILE(fdb_objects, "objects");
    DBFILE(fdb_index,   "index");

    if (force_textdump || stat(c_dir_binary, &statbuf) == F_FAILURE) {
	if (mkdir(c_dir_binary, READ_WRITE_EXECUTE) == F_FAILURE)
            FAIL("Cannot create binary directory \"%s\".\n");
    } else if (!S_ISDIR(statbuf.st_mode)) {
	if (unlink(c_dir_binary) == F_FAILURE)
	    FAIL("Cannot delete file \"%s\".\n");
	if (mkdir(c_dir_binary, READ_WRITE_EXECUTE) == F_FAILURE)
	    FAIL("Cannot create directory \"%s\".\n");
    }

    /* Check if binary/clean exists and contains the right version number. */
    if (!force_textdump) {
        fp = fopen(fdb_clean, "rb");
        if (fp) {
          if (fgets(buf, 80, fp) && atoi(buf) == VERSION_MAJOR) {
            if (fgets(buf, 80, fp) && atoi(buf) == VERSION_MINOR) {
              if (fgets(buf, 80, fp) && atoi(buf) == VERSION_PATCH) {
                if (fgets(buf, 80, fp) && atol(buf) == MAGIC_MODNUMBER) {
                    cnew = 0;
                    fgets(buf, 80, fp);
                    cur_search = atoi(buf);
                }
              }
            }
          }
          fclose(fp);
        }
    }

    database_file = fopen(fdb_objects, (cnew) ? "wb+" : "rb+");
    if (!database_file)
	FAIL("Cannot open object database file \"%s/objects\".\n");

    lookup_open(fdb_index, cnew);

    if (stat(fdb_objects, &statbuf) < 0)
	FAIL("Cannot stat database file \"%s/objects\".\n");

    bitmap_blocks = ROUND_UP(LOGICAL_BLOCK(statbuf.st_size) + DB_BITBLOCK, 8);
    bitmap = EMALLOC(char, (bitmap_blocks / 8)+1);
    memset(bitmap, 0, (bitmap_blocks / 8)+1);

    objnum = lookup_first_objnum();
    if (objnum >= db_top)
        db_top = objnum + 1;
    while (objnum != NOT_AN_IDENT) {
	if (!lookup_retrieve_objnum(objnum, &offset, &size))
	    fail_to_start("Database index is inconsistent.");

	if (objnum >= db_top)
	    db_top = objnum + 1;

	/* Mark blocks as busy in the bitmap. */
	db_mark(LOGICAL_BLOCK(offset), size);

	objnum = lookup_next_objnum();
    }

    /* If database is new, mark it as clean otherwise, it was clean already. */
    if (cnew)
	db_is_clean();
    else
	db_clean = 1;

    return cnew;
}

/* Grow the bitmap to given size. */
static void grow_bitmap(int new_blocks)
{
    new_blocks = ROUND_UP(new_blocks, 8);
    bitmap = EREALLOC(bitmap, char, (new_blocks / 8) + 1);
    memset(&bitmap[bitmap_blocks / 8], 0,
	   (new_blocks / 8) - (bitmap_blocks / 8));
    bitmap_blocks = new_blocks;
}

static void db_mark(off_t start, int size)
{
    int i, blocks;

    blocks = NEEDED(size, BLOCK_SIZE);

    while (start + blocks > bitmap_blocks)
	grow_bitmap(bitmap_blocks + DB_BITBLOCK);

    for (i = start; i < start + blocks; i++)
	bitmap[i >> 3] |= (1 << (i & 7));
}

static void db_unmark(off_t start, int size)
{
    int i, blocks;

    blocks = NEEDED(size, BLOCK_SIZE);

    /* Remember a free block was here. */
    last_free = start;

    for (i = start; i < start + blocks; i++) 
	bitmap[i >> 3] &= ~(1 << (i & 7));
}

static int db_alloc(int size)
{
    int blocks_needed, b, count, starting_block, over_the_top;

    b = last_free;
    blocks_needed = NEEDED(size, BLOCK_SIZE);
    over_the_top = 0;

    while (1) {
	if (b >= bitmap_blocks) {
	    /* Only wrap around once. */
	    if (!over_the_top) {
		b = 0;
		over_the_top = 1;
	    } else {
		grow_bitmap(b + DB_BITBLOCK);
	    }
	}

	starting_block = b;

	for (count = 0; count < blocks_needed; count++) {
	    if (bitmap[b >> 3] & (1 << (b & 7)))
		break;
	    b++;
	    if (b >= bitmap_blocks)
		grow_bitmap(b + DB_BITBLOCK);
	}

	if (count == blocks_needed) {
	    /* Mark these blocks taken and return the starting block. */
	    for (b = starting_block; b < starting_block + count; b++)
		bitmap[b >> 3] |= (1 << (b & 7));
	    last_free = b;
	    return starting_block;
	}

	b++;
    }
}

int db_get(object_t *object, long objnum)
{
    off_t offset;
    int size;

    /* Get the object location for the objnum. */
    if (!lookup_retrieve_objnum(objnum, &offset, &size))
	return 0;

    /* seek to location */
    if (fseek(database_file, offset, SEEK_SET))
	return 0;

    unpack_object(object, database_file);
    return 1;
}

int db_put(object_t *obj, long objnum)
{
    off_t old_offset, new_offset;
    int old_size, new_size = size_object(obj);

    db_is_dirty();

    if (lookup_retrieve_objnum(objnum, &old_offset, &old_size)) {
	if (NEEDED(new_size, BLOCK_SIZE) > NEEDED(old_size, BLOCK_SIZE)) {
	    db_unmark(LOGICAL_BLOCK(old_offset), old_size);
	    new_offset = BLOCK_OFFSET(db_alloc(new_size));
	} else {
	    new_offset = old_offset;
	}
    } else {
	new_offset = BLOCK_OFFSET(db_alloc(new_size));
    }

    if (!lookup_store_objnum(objnum, new_offset, new_size))
	return 0;

    if (fseek(database_file, new_offset, SEEK_SET)) {
	write_err("ERROR: Seek failed for %l.", objnum);
	return 0;
    }

    pack_object(obj, database_file);
    fflush(database_file);

    return 1;
}

int db_check(long objnum)
{
    off_t offset;
    int size;

    return lookup_retrieve_objnum(objnum, &offset, &size);
}

int db_del(long objnum)
{
    off_t offset;
    int size;

    /* Get offset and size of key. */
    if (!lookup_retrieve_objnum(objnum, &offset, &size))
	return 0;

    /* Remove key from location db. */
    if (!lookup_remove_objnum(objnum))
	return 0;

    db_is_dirty();

    /* Mark free space in bitmap */
    db_unmark(LOGICAL_BLOCK(offset), size);

    /* Mark object dead in file */
    if (fseek(database_file, offset, SEEK_SET)) {
	write_err("ERROR: Failed to seek to object %l.", objnum);
	return 0;
    }

    fputs("delobj", database_file);
    fflush(database_file);

    return 1;
}

void db_close(void)
{
    lookup_close();
    fclose(database_file);
    efree(bitmap);
    db_is_clean();
}

void db_flush(void)
{
    lookup_sync();
    db_is_clean();
}

static void db_is_clean(void)
{
    FILE *fp;

    if (db_clean)
	return;

    /* Create 'clean' file. */
    fp = open_scratch_file(c_clean_file, "w");
    if (!fp)
	panic("Cannot create file 'clean'.");

    fprintf(fp, "%d\n%d\n%d\n%li\n%li\n",
                VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
                (long) MAGIC_MODNUMBER, cur_search);
    fputs(SYSTEM_TYPE, fp);
    close_scratch_file(fp);
    db_clean = 1;
}

static void db_is_dirty(void) {
    if (db_clean) {
	/* Remove 'clean' file. */
	if (unlink(c_clean_file) == -1)
	    panic("Cannot remove file 'clean'.");
	db_clean = 0;
    }
}

/* checks for #1/$root and #0/$sys, adds them if they
   do not exist.  Call AFTER init_*_db has been called */

INTERNAL void _check_obj(long objnum, list_t * parents, char * name) {
    object_t * obj = cache_retrieve(objnum),
             * obj2;
    long       other;
    Ident      id = ident_get(name);
    
    if (!obj)
        obj = object_new(objnum, parents);

    if (lookup_retrieve_name(id, &other)) {
        if (other != objnum)
           printf("ACK: $%s is not bound to #%li!!, tweaking...\n",name,objnum);

        if ((obj2 = cache_retrieve(other))) {
            object_del_objname(obj2);
            cache_discard(obj2);
        }
    } 

    object_set_objname(obj, id);
    cache_discard(obj);
}

void init_core_objects(void) {
    data_t   * d;
    list_t   * parents;

    parents = list_new(0);
    _check_obj(ROOT_OBJNUM, parents, "root");
    list_discard(parents);

    parents = list_new(1);
    d = list_empty_spaces(parents, 1);
    d->type = OBJNUM;
    d->u.objnum = ROOT_OBJNUM;
    _check_obj(SYSTEM_OBJNUM, parents, "sys");
    list_discard(parents);
}
