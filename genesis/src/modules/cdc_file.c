/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_file.c
// ---
// Connection and File I/O module
*/

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "y.tab.h"
#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "memory.h"
#include "util.h"

/*
// Initialize a file path, make sure you can open it, etc
//
// return values:
//
//    0 if it failed and threw an error
//    1 if everything is O.K.
//
// Added: 30 Jul 95 - BJG
*/
internal int init_file_path(data_t      * name,
                            struct stat * statbuf,
                            char        * fname,
                            int           max)
{
    int    len;

    len = string_length(name->u.str);

    if (strstr(string_chars(name->u.str), "../") || len == 0) {
        cthrow(perm_id, "Filename %D is not legal.", name);
        return 0;
    }

    /* create a variable it will pull the base name from */
    strncpy(fname, "root/", max);
    strncat(fname, string_chars(name->u.str), max);

    /* Stat the file */
    if (stat(fname, statbuf) < 0) {
        cthrow(file_id, "Cannot find file %D.", name);
        return 0;
    }

    if (S_ISDIR(statbuf->st_mode)) {
        cthrow(directory_id, "File %D is a directory.", name);
        return 0;
    }

    return 1;
}

/*
// Get basic statistics on a file
//
// Added: 30 Jul 95 - BJG
*/
void op_stat_file(void) {
    struct stat    sbuf;
    register int   x;
    list_t       * s;
    data_t         * args,
                 * d;
    char           fname[BIGBUF];

    if (!func_init_1(&args, STRING))
        return;

    if (!init_file_path(&args[0], &sbuf, fname, BIGBUF)) {
        return;
    }

    s = list_new(5);
    d = list_empty_spaces(s, 5);
    for (x=0; x < 5; x++)
        d[x].type = INTEGER;

    d[0].u.val = (int) sbuf.st_mode;
    d[1].u.val = (int) sbuf.st_size;
    d[2].u.val = (int) sbuf.st_atime;
    d[3].u.val = (int) sbuf.st_mtime;
    d[4].u.val = (int) sbuf.st_ctime;

    push_list(s);
    list_discard(s);
}

/*
// Read a file into the db
//
// Added: 30 Jul 95 - BJG
*/
void op_read_file(void)
{
    size_t size, i, r;
    data_t *args;
    FILE *fp;
    char   fname[BIGBUF];
    Buffer *buf;
    struct stat statbuf;

    /* Accept the name of a file to read */
    if (!func_init_1(&args, STRING))
        return;

    if (!init_file_path(&args[0], &statbuf, fname, BIGBUF)) {
        return;
    }

    size = statbuf.st_size;

    /* Open the file for reading. */
    fp = open_scratch_file(fname, "r");
    pop(1);
    if (!fp) {
        cthrow(file_id, "Cannot open file %D for reading.", &args[0]);
        return;
    }

    /* Allocate a buffer to hold the file contents. */
    buf = buffer_new(size);

    /* Read in the file. */
    i = 0;
    while (i < size) {
        r = fread(buf->s + i, sizeof(unsigned char), size, fp);
        if (r <= 0) {
            buffer_discard(buf);
            close_scratch_file(fp);
            cthrow(file_id, "Trouble reading file %D.", &args[0]);
            return;
        }
        i += r;
    }

    /* return the buffer (wahoo, big memory chunks) */
    push_buffer(buf);

    /* Discard the buffer and close the file. */
    buffer_discard(buf);
    close_scratch_file(fp);
}

/*
// echo a file to the connection
*/
void op_echo_file(void)
{
    size_t size, i, r;
    data_t *args;
    FILE  * fp;
    char   fname[BIGBUF];
    Buffer *buf;
    struct stat statbuf;

    /* Accept the name of a file to echo. */
    if (!func_init_1(&args, STRING))
        return;

    /* Initialize the file */
    if (!init_file_path(&args[0], &statbuf, fname, BIGBUF)) {
        return;
    }

    pop(1);

    size = statbuf.st_size;

    /* Open the file for reading. */
    fp = open_scratch_file(fname, "r");
    if (!fp) {
        cthrow(file_id, "Cannot open file %D for reading.", &args[0]);
        return;
    }

    /* Allocate a buffer to hold the file contents. */
    buf = buffer_new(size);

    /* Read in the file. */
    i = 0;
    while (i < size) {
        r = fread(buf->s + i, sizeof(unsigned char), size, fp);
        if (r <= 0) {
            buffer_discard(buf);
            close_scratch_file(fp);
            cthrow(file_id, "Trouble reading file %D.", &args[0]);
            return;
        }
        i += r;
    }

    /* Write the file. */
    tell(cur_frame->object, buf);

    /* Discard the buffer and close the file. */
    buffer_discard(buf);
    close_scratch_file(fp);

    push_int(1);
}

