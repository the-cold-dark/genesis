/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Object storage routines.
//
// The block allocation algorithm in this code is due to Marcus J. Ranum.
*/

#define _binarydb_

#include "defs.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "cdc_db.h"
#include "util.h"
#include "moddef.h"

#define NEEDED(n, b)		(((n) % (b)) ? (n) / (b) + 1 : (n) / (b))
#define ROUND_UP(a, m)		(((a) - 1) + (m) - (((a) - 1) % (m)))

#define	BLOCK_SIZE		256		/* Default block size */
#define	DB_BITBLOCK		512		/* Bitmap growth in blocks */
#define	LOGICAL_BLOCK(off)	((off) / BLOCK_SIZE)
#define	BLOCK_OFFSET(block)	((block) * BLOCK_SIZE)

static void db_mark(off_t start, Int size);
static void db_unmark(off_t start, Int size);
static void grow_bitmap(Int new_blocks);
static Int db_alloc(Int size);
static void db_is_clean(void);
static void db_is_dirty(void);

static Int last_free = 0;	/* Last known or suspected free block */

static FILE *database_file = NULL;

static char *dump_bitmap  = NULL;
static Int   dump_blocks;
static Int   last_dumped;

static char *bitmap = NULL;
static Int bitmap_blocks = 0;
static Int allocated_blocks = 0;

char c_clean_file[255];

static Int db_clean;

extern Long cur_search, db_top;

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
        allocated_blocks=0; \
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
    Int           size,
                  outdated = 1;
    Long          objnum;

    /* make it '.clean' because too many people were innocently creating
       their own clean file, which then led them to use a corrupt db */
    sprintf(c_clean_file, "%s/.clean", c_dir_binary);
    DBFILE(fdb_clean,   ".clean");
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
It was compiled on %s with coldcc %d.%d-%d and module key %li\n\
This system is %s with coldcc %d.%d-%d and module key %li\n\n",
        c_dir_binary, buf, atoi(v_major), atoi(v_minor), atoi(v_patch),
        atol(magicmod), SYSTEM_TYPE, VERSION_MAJOR, VERSION_MINOR,
        VERSION_PATCH, (long) MAGIC_MODNUMBER);
        FAIL("Unable to load database \"%s\".\n");
    }

    open_db_objects("rb+");
    lookup_open(fdb_index, 0);
    init_bitmaps();
    sync_index();
    fprintf (errfile, "Binary database fragmentation: %.2f%%\n",
		100.0*(1.0-(float)allocated_blocks/(float)bitmap_blocks));

    db_clean = 1;
}

void init_new_db(void) {
    struct stat   statbuf;
    char          fdb_objects[LINE],
                  fdb_index[LINE];
    off_t         offset;
    Int           size;
    Long          objnum;

    sprintf(c_clean_file, "%s/.clean", c_dir_binary);
    DBFILE(fdb_objects, "objects");
    DBFILE(fdb_index,   "index");

    open_db_directory();
    open_db_objects("w+");
    lookup_open(fdb_index, 1);
    init_bitmaps();
    sync_index();
    db_is_clean();
}

Int init_db(Int force_textdump) {
    struct stat   statbuf;
    FILE        * fp;
    char          buf[LINE],
                  fdb_clean[LINE],
                  fdb_objects[LINE],
                  fdb_index[LINE];
    off_t         offset;
    Int           cnew = 1,
                  size;
    Long          objnum;

    sprintf(c_clean_file, "%s/.clean", c_dir_binary);
    DBFILE(fdb_clean,   ".clean");
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
static void grow_bitmap(Int new_blocks)
{
    new_blocks = ROUND_UP(new_blocks, 8);
    bitmap = EREALLOC(bitmap, char, (new_blocks / 8) + 1);
    memset(&bitmap[bitmap_blocks / 8], 0,
	   (new_blocks / 8) - (bitmap_blocks / 8));
    bitmap_blocks = new_blocks;
}

static void db_mark(off_t start, Int size)
{
    Int i, blocks;

    blocks = NEEDED(size, BLOCK_SIZE);
    allocated_blocks+=blocks;

    while (start + blocks > bitmap_blocks)
	grow_bitmap(bitmap_blocks + DB_BITBLOCK);

    for (i = start; i < start + blocks; i++)
	bitmap[i >> 3] |= (1 << (i & 7));
}

/* This routine copies the object from the current binary to the
   dump binary. It will first check whether copying is needed.
   Called from db_unmark and db_put (to prevent dirtying the undumped
   objects) */

static void dump_copy (off_t start, Int blocks)
{
    Int i;
    char buf[BLOCK_SIZE];

    /* check if we need to do this */
    for (i=start; i<start+blocks; i++) {
	if (i < dump_blocks && (bitmap[i >> 3] & (1 << (i&7))))
	    break;
    }

    if (i == start+blocks) return;

    if (fseek(database_file, BLOCK_OFFSET (start), SEEK_SET))
        panic("fseek(\"%s\") in copy: %s", database_file, strerror(errno));

    /* PORTABILITY WARNING : THIS FSEEK MAKES THE FILE LONGER IN SOME CASES.
       Checked on Solaris, should work on others. */

    if (fseek(dump_db_file,  BLOCK_OFFSET (start), SEEK_SET))
        panic("fseek(\"%s\") in copy: %s", dump_db_file, strerror(errno));
    for (i=0; i<blocks; i+=16) {
	fread (buf, 1, BLOCK_SIZE, database_file);
	fwrite (buf, 1, BLOCK_SIZE, dump_db_file);
	dump_bitmap[i >> 3] &= ~(1 << (i&7));
    }
}

/* open the dump database. return -1 on failure (can't open the file),
   -2 -> we are already dumping */

Int db_start_dump(char *dump_objects_filename) {
    if (dump_db_file)
	return -2;
    dump_db_file = fopen(dump_objects_filename, "wb+");
    if (!dump_db_file)
	return -1;
    last_dumped = 0;
    dump_blocks = bitmap_blocks;
    dump_bitmap = EMALLOC(char, (bitmap_blocks / 8)+1);
    memcpy(dump_bitmap, bitmap, (bitmap_blocks / 8)+1);
    return 0;
}

/* this is the main hook. It's supposed to be called from the main loop, with
   the maximal number of blocks you want to dump.
   return: 0 -> either dump continues, or we weren't dumping before
           1 -> dump finished, -1 -> unspecified error
	   call it with maxblocks = between 8 and 64 */

Int dump_some_blocks (Int maxblocks)
{
    Int dofseek = 1;
    char buf[BLOCK_SIZE];

    if (!dump_db_file)
	return DUMP_NOT_IN_PROGRESS;
    while (maxblocks) {
	if ( (dump_bitmap[last_dumped >> 3] & (1 << (last_dumped & 7))) ) {
	    if (dofseek) {
		if (fseek(database_file, BLOCK_OFFSET (last_dumped), SEEK_SET))
                   panic("fseek(\"%s\"..): %s", database_file, strerror(errno));
		if (fseek(dump_db_file,  BLOCK_OFFSET (last_dumped), SEEK_SET))
                   panic("fseek(\"%s\"..): %s", dump_db_file, strerror(errno));
		dofseek=0;
	    }
	    fread (buf, 1, BLOCK_SIZE, database_file);
	    fwrite (buf, 1, BLOCK_SIZE, dump_db_file);
	    dump_bitmap[last_dumped >> 3] &= ~(1 << (last_dumped & 7));
	    maxblocks--;
	}
	else
	    dofseek=1;

	if (last_dumped++ >= dump_blocks) {
	    if (fclose (dump_db_file))
	        panic("Unable to close dump file '%s'", dump_db_file);
	    dump_db_file = NULL;
	    free (dump_bitmap);
	    dump_bitmap=NULL;
	    return DUMP_FINISHED;
	}
    }
    return DUMP_DUMPED_BLOCKS;
}

static void db_unmark(off_t start, Int size)
{
    Int i, blocks;

    blocks = NEEDED(size, BLOCK_SIZE);
    allocated_blocks-=blocks;

    if (dump_db_file) dump_copy (start, blocks);

    /* Remember a free block was here. */
    last_free = start;

    for (i = start; i < start + blocks; i++)
	bitmap[i >> 3] &= ~(1 << (i & 7));
}

static Int db_alloc(Int size)
{
    Int blocks_needed, b, count, starting_block, over_the_top;

    b = last_free;
    blocks_needed = NEEDED(size, BLOCK_SIZE);
    over_the_top = 0;

    while (1) {

	if (b < bitmap_blocks && bitmap[b >> 3] == (char)255) {
	    /* 8 full blocks. Let's run away from this! */
	    b = (b & ~7) + 8;
	    while (b < bitmap_blocks && bitmap[b >> 3] == (char)255) {
		b += 8;
	    }
	}

	if (b >= bitmap_blocks) {
	    /* Only wrap around once. */
	    if (!over_the_top) {
		b = 0;
		over_the_top = 1;
		continue;
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
		/* time to wrap around if we still haven't */
		if (!over_the_top) {
		    b=0;
		    over_the_top=1;
		    break;
		} else
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

Int db_get(Obj *object, Long objnum)
{
    off_t offset;
    Int size;

    /* Get the object location for the objnum. */
    if (!lookup_retrieve_objnum(objnum, &offset, &size))
	return 0;

    /* seek to location */
    if (fseek(database_file, offset, SEEK_SET))
	return 0;

    unpack_object(object, database_file);
    return 1;
}

Int db_put(Obj *obj, Long objnum)
{
    off_t old_offset, new_offset;
    Int old_size, new_size = size_object(obj);

    db_is_dirty();

    if (lookup_retrieve_objnum(objnum, &old_offset, &old_size)) {
	if (NEEDED(new_size, BLOCK_SIZE) > NEEDED(old_size, BLOCK_SIZE)) {
	    db_unmark(LOGICAL_BLOCK(old_offset), old_size);
	    new_offset = BLOCK_OFFSET(db_alloc(new_size));
	} else {
	    if (dump_db_file)
		dump_copy (LOGICAL_BLOCK(old_offset),
			   NEEDED(old_size, BLOCK_SIZE));
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

Int db_check(Long objnum)
{
    off_t offset;
    Int size;

    return lookup_retrieve_objnum(objnum, &offset, &size);
}

Int db_del(Long objnum)
{
    off_t offset;
    Int size;

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

void finish_backup(void) {
    FILE * fp;
    char buf[BUF];
    
    strcpy(buf, c_dir_binary);
    strcat(buf, ".bak/.clean");
    fp = open_scratch_file(buf, "w");
    if (!fp)
        panic("Cannot create file 'clean'.");
    fprintf(fp, "%d\n%d\n%d\n%li\n%li\n",
                VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
                (long) MAGIC_MODNUMBER, cur_search);
    fputs(SYSTEM_TYPE, fp);
    close_scratch_file(fp);
}

static void db_is_clean(void) {
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

INTERNAL void _check_obj(Long objnum, cList * parents, char * name) {
    Obj * obj = cache_retrieve(objnum),
             * obj2;
    Long       other;
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
    cData   * d;
    cList   * parents;

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

#undef _binarydb_
