/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Object storage routines.
//
// The block allocation algorithm in this code is due to Marcus J. Ranum.
*/

#define _binarydb_

#include "defs.h"

#ifdef __UNIX__
#include <sys/param.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "cdc_types.h"
#include "cdc_string.h"
#include "buffer.h"

#ifdef USE_CLEANER_THREAD
pthread_mutex_t db_mutex;

#ifdef DEBUG_DB_LOCK
#define LOCK_DB(func) \
	write_err("%s: locking db", func); \
	pthread_mutex_lock(&db_mutex); \
	write_err("%s: locked db", func);
#define UNLOCK_DB(func) \
	pthread_mutex_unlock(&db_mutex); \
	write_err("%s: unlocked db", func);
#else /* thread, no debugging */
#define LOCK_DB(func) \
	pthread_mutex_lock(&db_mutex);
#define UNLOCK_DB(func) \
	pthread_mutex_unlock(&db_mutex);
#endif
#else /* no thread */
#define LOCK_DB(func)
#define UNLOCK_DB(func)
#endif

#include "cdc_db.h"
#include "util.h"
#include "moddef.h"

#ifdef __MSVC__
#include <direct.h>
#endif

/* suggested by xmath as possibly faster
#define NEEDED(n, b)            (((n) + ((b) - 1)) / (b))
#define ROUND_UP(n, b)          (((n) + ((b) - 1)) % (b))
*/

#define NEEDED(n, b)		(((n) % (b)) ? (n) / (b) + 1 : (n) / (b))
#define ROUND_UP(a, m)		(((a) - 1) + (m) - (((a) - 1) % (m)))

#define	BLOCK_SIZE		256		/* Default block size */
#define	DB_BITBLOCK		10240		/* Bitmap growth in blocks */
#define	LOGICAL_BLOCK(off)	((off) / BLOCK_SIZE)
#define	BLOCK_OFFSET(block)	((block) * BLOCK_SIZE)

static void simble_mark(off_t start, Int size);
static void simble_unmark(off_t start, Int size);
static void simble_grow_bitmap(Int new_blocks);
static Int  simble_alloc(Int size);
static void simble_flag_as_clean(void);
static void simble_flag_as_dirty(void);
static void simble_verify_clean(void);

static Int last_free = 0;	/* Last known or suspected free block */

static FILE *database_file = NULL;

static char *dump_bitmap  = NULL;
static Int   dump_blocks;
static off_t last_dumped;

static char *bitmap = NULL;
static Int bitmap_blocks = 0;
static Int allocated_blocks = 0;

static char c_clean_file[255];

static Int db_clean;
static cStr *pad_string;

extern Long db_top;
extern Long num_objects;

/* this isn't the most graceful way, but *shrug* */
#define WARN(_s_) { \
	fprintf(errfile, _s_, c_dir_binary); \
	if (errfile != stderr) \
	    fprintf(stderr, _s_, c_dir_binary); \
    }

#define FAIL(_s_) { WARN(_s_) exit(1); }

#define DBFILE(__b, __f) (sprintf(__b, "%s/%s", c_dir_binary, __f))

#ifdef __MSVC__
#define open_db_directory() { \
        if (stat(c_dir_binary, &statbuf) == F_FAILURE) { \
            if (mkdir(c_dir_binary) == F_FAILURE) \
                FAIL("Cannot create binary directory \"%s\".\n"); \
        } else if (!S_ISDIR(statbuf.st_mode)) { \
            if (unlink(c_dir_binary) == F_FAILURE) \
                FAIL("Cannot delete file \"%s\".\n"); \
            if (mkdir(c_dir_binary) == F_FAILURE) \
                FAIL("Cannot create directory \"%s\".\n"); \
        } \
    }
#else
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
#endif

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
	    ++num_objects; \
            if (!lookup_retrieve_objnum(objnum, &offset, &size)) \
                FAIL("Database index (\"%s/index\") is inconsistent.\n"); \
            if (objnum >= db_top) \
                db_top = objnum + 1; \
            simble_mark(LOGICAL_BLOCK(offset), size); \
            objnum = lookup_next_objnum(); \
        } \
    }

#define open_db_objects(__p) { \
        database_file = fopen(fdb_objects, __p); \
        if (!database_file) \
            FAIL("Cannot open object database file \"%s/objects\".\n"); \
    }

#ifndef __Win32__
static Bool good_perms(struct stat * sb) {
    if (!geteuid())
        return YES;
    if (sb->st_uid == geteuid() && (sb->st_mode & S_IRWXU))
        return YES;
    else if (sb->st_gid == getegid() && (sb->st_mode & S_IRWXG))
        return YES;
    return NO;
}
#endif

static void simble_verify_clean(void) {
    Bool isdirty = YES;
    char system[LINE],
         v_major[LINE],
         v_minor[LINE],
         v_patch[LINE],
         magicmod[LINE],
         search[LINE];
    char * s;
    FILE * fp;

    v_major[0] = v_minor[0] = v_patch[0] = magicmod[0] =
        system[0] = search[0] = '\0';

    if ((fp = fopen(c_clean_file, "rb"))) {
        fgets(system, LINE, fp);
        fgets(v_major, LINE, fp);
        fgets(v_minor, LINE, fp);
        fgets(v_patch, LINE, fp);
        fgets(magicmod, LINE, fp);
        fgets(search, LINE, fp);

        /* cleanup anything after the system name */
        s = &system[strlen(system)-1];
        while (s > system && isspace(*s)) {
            *s = '\0';
            s--;
        }

        /* do the check.. */
        if (atoi(v_major) == VERSION_MAJOR) {
            if (atoi(v_minor) == VERSION_MINOR) {
                if (atoi(v_patch) == VERSION_PATCH) {
                    if (atol(magicmod) == MAGIC_MODNUMBER) {
                        if (strcmp(system, SYSTEM_TYPE) == 0) {
                            isdirty = NO; /* yay */
                        }
                    }
                }
            }
        }

        fclose(fp);
    } else {
        FAIL("Binary database (\"%s\") is corrupted, aborting...\n");
    }

    if (isdirty) {
        fprintf(stderr, "** Binary database \"%s\" is incompatible, systems:\n"
                        "** it:   <%s> %d.%d-%d (module key %li)\n"
                        "** this: <%s> %d.%d-%d (module key %li)\n",
                c_dir_binary, system, atoi(v_major), atoi(v_minor),
		atoi(v_patch), atol(magicmod), SYSTEM_TYPE, VERSION_MAJOR,
		VERSION_MINOR, VERSION_PATCH, (long) MAGIC_MODNUMBER);
        FAIL("Unable to load database \"%s\": incompatible.\n");
    }
}

void init_binary_db(void) {
    struct stat   statbuf;
    char          fdb_objects[BUF],
                  fdb_index[BUF];
    off_t         offset;
    Int           size;
    cObjnum       objnum;

#ifdef USE_CLEANER_THREAD
    pthread_mutex_init (&db_mutex, NULL);
#endif

    pad_string = string_of_char(0, 256);
    sprintf(c_clean_file, "%s/.clean", c_dir_binary);
    DBFILE(fdb_objects, "objects");
    DBFILE(fdb_index,   "index");

    if (stat(c_dir_binary, &statbuf) == F_FAILURE)
        FAIL("Cannot find binary directory \"%s\".\n")
    else if (!S_ISDIR(statbuf.st_mode))
        FAIL("Binary db \"%s\" is not a directory.\n")

#ifndef __Win32__
    else if (!good_perms(&statbuf))
        FAIL("Cannot write to binary directory \"%s\".\n")

    /* whine a little bit */
    if (statbuf.st_mode & S_IWOTH)
        WARN("Binary directory \"%s\" is writable by ANYBODY\n")
#endif

    /* check the clean file */
    simble_verify_clean();

    open_db_objects("rb+");
    lookup_open(fdb_index, 0);
    init_bitmaps();
    sync_index();
    fprintf (errfile, "[%s] Binary database free space: %.2f%%\n",
             timestamp(NULL), (100.0 * simble_fragmentation()));

    db_clean = 1;
}

void init_new_db(void) {
    struct stat   statbuf;
    char          fdb_objects[BUF],
                  fdb_index[BUF];
    off_t         offset;
    Int           size;
    cObjnum       objnum;

#ifdef USE_CLEANER_THREAD
    pthread_mutex_init (&db_mutex, NULL);
#endif
    LOCK_DB("init_new_db")

    pad_string = string_of_char(0, 256);
    sprintf(c_clean_file, "%s/.clean", c_dir_binary);
    DBFILE(fdb_objects, "objects");
    DBFILE(fdb_index,   "index");

    open_db_directory();
    open_db_objects("wb+");
    lookup_open(fdb_index, 1);
    init_bitmaps();
    sync_index();
    simble_flag_as_clean();
    UNLOCK_DB("init_new_db")
}

#ifdef DEBUG
static void display_bitmap()
{
    Int i=0, j=0;
    char line[80];
    static char *hex="0123456789ABCDEF";

    memset(line, 0, sizeof(line));
    while (i < (bitmap_blocks/8))
    {
	line[j++] = hex[(bitmap[i] & 0xF0) >> 4];
	line[j++] = hex[bitmap[i] & 0x0F];
	i++;
	if (!(i%4))
	    line[j++] = ' ';
	if (!(i%16))
	{
	    line[j++] = 0;
	    write_err("%s", line);
	    j = 0;
	    memset(line, 0, sizeof(line));
	}
    }
}
#endif

/* Grow the bitmap to given size. */
static void simble_grow_bitmap(Int new_blocks)
{
    new_blocks = ROUND_UP(new_blocks, 8);
    bitmap = EREALLOC(bitmap, char, (new_blocks / 8) + 1);
    memset(&bitmap[bitmap_blocks / 8], 0, (new_blocks / 8) - (bitmap_blocks / 8));
    bitmap_blocks = new_blocks;
}

static void simble_mark(off_t start, Int size)
{
    Int i, blocks;

    blocks = NEEDED(size, BLOCK_SIZE);
    allocated_blocks += blocks;

    while (start + blocks > bitmap_blocks)
	simble_grow_bitmap(bitmap_blocks + DB_BITBLOCK);

    for (i = start; i < start + blocks; i++)
	bitmap[i >> 3] |= (1 << (i & 7));
}

/* This routine copies the object from the current binary to the
   dump binary. It will first check whether copying is needed.
   Called from simble_unmark and simble_put (to prevent dirtying
   the undumped objects) */

static void dump_copy (off_t start, Int blocks)
{
#if USE_OLD_DUMP_COPY
    Int i;
    char buf[BLOCK_SIZE];

    /* check if we need to do this */
    for (i=start; i<start+blocks; i++) {
	if (i < dump_blocks && (bitmap[i >> 3] & (1 << (i&7))))
	    break;
    }

    if (i == start+blocks) return;

    if (fseeko(database_file, BLOCK_OFFSET (start), SEEK_SET)) {
	UNLOCK_DB("dump_copy")
        panic("fseeko(\"%s\") in copy: %s", database_file, strerror(errno));
    }

    /* PORTABILITY WARNING : THIS FSEEK MAKES THE FILE LONGER IN SOME CASES.
       Checked on Solaris, should work on others. */

    if (fseeko(dump_db_file,  BLOCK_OFFSET (start), SEEK_SET)) {
	UNLOCK_DB("dump_copy")
        panic("fseeko(\"%s\") in copy: %s", dump_db_file, strerror(errno));
    }
    for (i=0; i<blocks; i++) {
	fread (buf, 1, BLOCK_SIZE, database_file);
	fwrite (buf, 1, BLOCK_SIZE, dump_db_file);
	dump_bitmap[(start+i) >> 3] &= ~(1 << ((start+i)&7));
    }
#else
    off_t block = 0;
    Int   dofseek = 1;
    char  buf[BLOCK_SIZE];

    block = start;
    blocks += start;
    if (blocks > dump_blocks)
        blocks = dump_blocks;

    while (block < blocks) {                                                                
        if ( (dump_bitmap[block >> 3] & (1 << (block & 7))) ) {
            if (dofseek) {
                if (fseeko(database_file, BLOCK_OFFSET (block), SEEK_SET)) {
                    UNLOCK_DB("dump_copy")
                    panic("fseeko(\"%s\"..): %s", database_file, strerror(errno));
                }
                if (fseeko(dump_db_file,  BLOCK_OFFSET (block), SEEK_SET)) {
                    UNLOCK_DB("dump_copy")
                    panic("fseeko(\"%s\"..): %s", dump_db_file, strerror(errno));
                }
                dofseek=0;
            }
            fread (buf, 1, BLOCK_SIZE, database_file);
            fwrite (buf, 1, BLOCK_SIZE, dump_db_file);
            dump_bitmap[block >> 3] &= ~(1 << (block & 7));
        }
        else
            dofseek=1;

        ++block;
    }
#endif
}

/* open the dump database. return -1 on failure (can't open the file),
   -2 -> we are already dumping */

Int simble_dump_start(char *dump_objects_filename) {
    if (dump_db_file)
	return -2;
    dump_db_file = fopen(dump_objects_filename, "wb+");
    if (!dump_db_file)
	return -1;
    last_dumped = 0;

    LOCK_DB("simble_dump_start")

    dump_blocks = bitmap_blocks;
    dump_bitmap = EMALLOC(char, (bitmap_blocks / 8)+1);
    memcpy(dump_bitmap, bitmap, (bitmap_blocks / 8)+1);

    UNLOCK_DB("simble_dump_start")

    return 0;
}

/* this is the main hook. It's supposed to be called from the main loop, with
   the maximal number of blocks you want to dump.
   return: 0 -> either dump continues, or we weren't dumping before
           1 -> dump finished, -1 -> unspecified error
	   call it with maxblocks = between 8 and 64 */

Int simble_dump_some_blocks (Int maxblocks)
{
    Int dofseek = 1;
    char buf[BLOCK_SIZE];

    if (!dump_db_file)
	return DUMP_NOT_IN_PROGRESS;

    LOCK_DB("simble_dump_some_blocks")

    while (maxblocks) {
	if ( (dump_bitmap[last_dumped >> 3] & (1 << (last_dumped & 7))) ) {
	    if (dofseek) {
		if (fseeko(database_file, BLOCK_OFFSET (last_dumped), SEEK_SET)) {
		    UNLOCK_DB("simble_dump_some_blocks")
                    panic("fseeko(\"%s\"..): %s", database_file, strerror(errno));
		}
		if (fseeko(dump_db_file,  BLOCK_OFFSET (last_dumped), SEEK_SET)) {
		    UNLOCK_DB("simble_dump_some_blocks")
                    panic("fseeko(\"%s\"..): %s", dump_db_file, strerror(errno));
		}
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
	    if (fclose (dump_db_file)) {
		UNLOCK_DB("simble_dump_some_blocks")
	        panic("Unable to close dump file '%s'", dump_db_file);
	    }
	    dump_db_file = NULL;
	    free (dump_bitmap);
	    dump_bitmap=NULL;

	    UNLOCK_DB("simble_dump_some_blocks")

	    return DUMP_FINISHED;
	}
    }

    UNLOCK_DB("simble_dump_some_blocks")

    return DUMP_DUMPED_BLOCKS;
}

static void simble_unmark(off_t start, Int size)
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

static Int simble_alloc(Int size)
{
    Int blocks_needed, b, count, starting_block, over_the_top;

    b = last_free;
    blocks_needed = NEEDED(size, BLOCK_SIZE);
#ifdef BUILDING_COLDCC
    over_the_top = 1;
#else
    over_the_top = 0;
#endif

    for (;;) {

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
		simble_grow_bitmap(b + DB_BITBLOCK);
	    }
	}

	starting_block = b;

	for (count = 0; count < blocks_needed; count++) {
	    if (bitmap[b >> 3] & (1 << (b & 7)))
		break;
	    b++;
	    if (b >= bitmap_blocks) {
		/* time to wrap around if we still haven't */
		if (!over_the_top) {
		    b = 0;
		    over_the_top = 1;
		    break;
		} else {
		    simble_grow_bitmap(b + ROUND_UP(blocks_needed-count, DB_BITBLOCK));
                }
            }
	}

	if (count == blocks_needed) {
	    /* Mark these blocks taken and return the starting block. */
	    allocated_blocks+=count;
	    for (b = starting_block; b < starting_block + count; b++)
		bitmap[b >> 3] |= (1 << (b & 7));
	    last_free = b;
	    return starting_block;
	}

	b++;
    }
}

Int simble_get(Obj *object, cObjnum objnum, Long *sizeread)
{
    off_t offset;
    Int size;
    cBuf *buf;
    Long buf_pos;

    if (sizeread)
        *sizeread = -1;

    /* Get the object location for the objnum. */
    if (!lookup_retrieve_objnum(objnum, &offset, &size))
	return 0;

    LOCK_DB("simble_get")

    /* seek to location */
    if (fseeko(database_file, offset, SEEK_SET)) {
	UNLOCK_DB("simble_get")
	return 0;
    }

    if (sizeread)
        *sizeread = size;
    buf = buffer_new(size);
    buf->len = size;
    buf_pos = fread(buf->s, sizeof(uChar), size, database_file);
    UNLOCK_DB("simble_get")
    if (buf_pos != size)
        panic("simble_get: only read %d of %d bytes.", buf_pos, size);

    buf_pos = 0;
    unpack_object(buf, &buf_pos, object);
    buffer_discard(buf);

    return 1;
}

static Int check_free_blocks(Int blocks_needed, Int b)
{
    Int count;
    
    if (b >= bitmap_blocks)
	return 0;
    for (count = 0; count < blocks_needed; count++) {
	if (bitmap[b >> 3] & (1 << (b & 7)))
	    break;
	b++;
	if (b >= bitmap_blocks)
	    break;
    }
    return count == blocks_needed;
}

Int simble_put(Obj *obj, cObjnum objnum, Long *sizewritten)
{
    cBuf *buf;
    off_t old_offset, new_offset;
    Int old_size, new_size, tmp1, tmp2;

    old_offset = -1;
    if (lookup_retrieve_objnum(objnum, &old_offset, &old_size)) {
        buf = buffer_new(old_size);
        buf = pack_object(buf, obj);
	if (buf->len % BLOCK_SIZE)
	    buf = buffer_append_uchars_single_ref(buf, pad_string->s, 256 - (buf->len % BLOCK_SIZE));
	new_size = buf->len;

        LOCK_DB("simble_put")
        simble_flag_as_dirty();

	if ((tmp1=NEEDED(new_size, BLOCK_SIZE)) > (tmp2=NEEDED(old_size, BLOCK_SIZE))) {
	    /* check for the possible realloc */
	    if (check_free_blocks(tmp1 - tmp2, LOGICAL_BLOCK(old_offset)+tmp2)) {
		/* no, we don't have to move, just overwrite */
		if (dump_db_file)
		    dump_copy (LOGICAL_BLOCK(old_offset), tmp1);
		simble_mark(LOGICAL_BLOCK(old_offset) + tmp2,
			BLOCK_SIZE * (tmp1 - tmp2));
		new_offset = old_offset;
	    } else {
		simble_unmark(LOGICAL_BLOCK(old_offset), old_size);
		new_offset = BLOCK_OFFSET((off_t)simble_alloc(new_size));
	    }
        } else {
	    if (dump_db_file)
		dump_copy (LOGICAL_BLOCK(old_offset), tmp2);
	    if (tmp1 < tmp2) {
		simble_unmark(LOGICAL_BLOCK(old_offset) + tmp1,
			  BLOCK_SIZE * (tmp2 - tmp1));
	    }
	    new_offset = old_offset;
	}
    } else {
	++num_objects;
	buf = buffer_new(0);
	buf = pack_object(buf, obj);
	if (buf->len % BLOCK_SIZE)
	    buf = buffer_append_uchars_single_ref(buf, pad_string->s, 256 - (buf->len % BLOCK_SIZE));
	new_size = buf->len;

        LOCK_DB("simble_put")
        simble_flag_as_dirty();

	new_offset = BLOCK_OFFSET((off_t)simble_alloc(new_size));
    }

    /* Don't store it if it hasn't changed! */
    if ((new_offset != old_offset) ||
      (new_size   != old_size)) {
        if (!lookup_store_objnum(objnum, new_offset, new_size)) {
	    UNLOCK_DB("simble_put")
	    buffer_discard(buf);
            if (sizewritten) *sizewritten = 0;
	    return 0;
        }
    }

    if (fseeko(database_file, new_offset, SEEK_SET)) {
	UNLOCK_DB("simble_put")
	buffer_discard(buf);
	write_err("ERROR: Seek failed for %l.", objnum);
        if (sizewritten) *sizewritten = 0;
	return 0;
    }

    old_size = fwrite(buf->s, sizeof(uChar), new_size, database_file);
    buffer_discard(buf);
    fflush(database_file);
    UNLOCK_DB("simble_put")
    if (old_size != new_size)
        panic("simble_put: only wrote %d of %d bytes.", old_size, new_size);

    if (sizewritten) *sizewritten = new_size;

    return 1;
}

Int simble_check(cObjnum objnum)
{
    off_t offset;
    Int size;

    return lookup_retrieve_objnum(objnum, &offset, &size);
}

Int simble_del(cObjnum objnum)
{
    off_t offset;
    Int size;
    cBuf *buf;

    /* Get offset and size of key. */
    if (!lookup_retrieve_objnum(objnum, &offset, &size))
	return 0;

    /* Remove key from location db. */
    if (!lookup_remove_objnum(objnum))
	return 0;

    LOCK_DB("simble_del")

    --num_objects;
    simble_flag_as_dirty();

    /* Mark free space in bitmap */
    simble_unmark(LOGICAL_BLOCK(offset), size);

    /* Mark object dead in file */
    if (fseeko(database_file, offset, SEEK_SET)) {
	write_err("ERROR: Failed to seek to object %l.", objnum);

        UNLOCK_DB("simble_del")

	return 0;
    }

    buf = buffer_new(size);
    buf->len = size;
    memset(buf->s, 0, size);
    fwrite(buf->s, sizeof(uChar), size, database_file);
    buffer_discard(buf);
    fflush(database_file);

    UNLOCK_DB("simble_del")

    return 1;
}

void simble_close(void)
{
    LOCK_DB("simble_close")
    lookup_close();
    fclose(database_file);
    efree(bitmap);
    simble_flag_as_clean();
    string_discard(pad_string);
    UNLOCK_DB("simble_close")
}

void simble_flush(void)
{
    lookup_sync();

    LOCK_DB("simble_flush")

    simble_flag_as_clean();

    UNLOCK_DB("simble_flush")
}

#define write_clean_file(_fp_) \
    fprintf(_fp_, "%s\n%d\n%d\n%d\n%li\n", SYSTEM_TYPE, \
                VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,\
                (long) MAGIC_MODNUMBER)\

void simble_dump_finish(void) {
    FILE * fp;
    char buf[BUF];
    
    strcpy(buf, c_dir_binary);
    strcat(buf, ".bak/.clean");
    fp = open_scratch_file(buf, "wb");
    if (!fp)
        panic("Cannot create file 'clean'.");
    write_clean_file(fp);
    close_scratch_file(fp);
}

static void simble_flag_as_clean(void) {
    FILE *fp;

    if (db_clean)
	return;

    /* Create 'clean' file. */
    fp = open_scratch_file(c_clean_file, "wb");
    if (!fp) {
	UNLOCK_DB("simble_flag_as_clean")
	panic("Cannot create file 'clean'.");
    }
    write_clean_file(fp);
    close_scratch_file(fp);
    db_clean = 1;
}

static void simble_flag_as_dirty(void) {
    if (db_clean) {
	/* Remove 'clean' file. */
	if (unlink(c_clean_file) == -1) {
	    UNLOCK_DB("simble_flag_as_clean")
	    panic("Cannot remove file 'clean'.");
	}
	db_clean = 0;
    }
}

/* checks for #1/$root and #0/$sys, adds them if they
   do not exist.  Call AFTER init_*_db has been called */

static void _check_obj(cObjnum objnum, cList * parents, char * name) {
    Obj      * obj = cache_retrieve(objnum),
             * obj2;
    cObjnum    other;
    Ident      id = ident_get(name);

    if (!obj)
        obj = object_new(objnum, parents);

    if (lookup_retrieve_name(id, &other)) {
        if (other != objnum)
           printf("ACK: $%s is not bound to #%li!!, tweaking...\n",
                  name, (long)objnum);

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

Float simble_fragmentation(void) {
    return 1.0 - ((float)allocated_blocks/(float)bitmap_blocks);
}

#undef _binarydb_
