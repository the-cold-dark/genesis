/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
*/

#include "config.h"
#include "defs.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>  /* directory funcs */
#include "memory.h"
#include "cache.h"
#include "log.h"       /* op_dblog() */
#include "execute.h"
#include "binarydb.h"

void func_dblog(void) {
    data_t * args;

    /* Accept a string. */
    if (!func_init_1(&args, STRING))
        return;

    write_log("%S", args[0].u.str);
    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// Modifies: The object cache, identifier table, and binary database
//           files via cache_sync() and ident_dump().
// Effects: If called by the sytem object with no arguments,
//          performs a binary dump, ensuring that the files db and
//          db are consistent.  Returns 1 if the binary dump
//          succeeds, or 0 if it fails.
//
*/

#ifndef MAXBSIZE
#define MAXBSIZE 16384
#endif

#define x_THROW(_file_) { \
        cthrow(file_id, "%s: %s", _file_, strerror(errno)); \
        return 0; \
    }

INTERNAL int backup_file(char * file) {
    static char buf[MAXBSIZE];
    int from_fd, rcount, rval=1, to_fd, wcount;
    char source[BUF], dest[BUF];

    strcpy(source, c_dir_binary);
    strcat(source, "/");
    strcat(source, file);

    from_fd = open(source, O_RDONLY, 0);
    if (from_fd == F_FAILURE)
        x_THROW(source)

    strcpy(dest, c_dir_binary);
    strcat(dest, ".bak/");
    strcat(dest, file);

    to_fd = open(dest, (O_WRONLY|O_TRUNC|O_CREAT), (S_IRUSR|S_IWUSR));
    if (to_fd == F_FAILURE)
        x_THROW(dest)

    while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
        wcount = write(to_fd, buf, rcount);
        if (rcount != wcount) {
            cthrow(file_id, "Error on copying %s to %s", source, dest);
            rval = 0;
            break;
        } else if (wcount == F_FAILURE) {
            cthrow(file_id, "%s", strerror(errno));
            rval = 0;
            break;
        }
    }

    if (rcount == F_FAILURE && rval) {
        cthrow(file_id, "%s", strerror(errno));
        rval = 0;
    }

    (void)close(from_fd);
    if (close(to_fd) && rval) {
        cthrow(file_id, "%s", strerror(errno));
        rval = 0;
    }

    return rval;
}

void func_backup(void) {
    char          buf[BUF];
    struct stat   statbuf;
    struct dirent * dent;
    DIR      * dp;

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* get binary.bak, make sure its ours */
    strcpy(buf, c_dir_binary);
    strcat(buf, ".bak");
    if (stat(buf, &statbuf) == F_FAILURE) {
        if (mkdir(buf, READ_WRITE_EXECUTE) == F_FAILURE)
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(errno)))
    } else if (!S_ISDIR(statbuf.st_mode)) {
        if (unlink(buf) == F_FAILURE)
            THROW((file_id, "Cannot delete file \"%s\": %s", buf, strerror(errno)))
        if (mkdir(buf, READ_WRITE_EXECUTE) == F_FAILURE)
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(errno)))
    }

    /* sync the db */
    cache_sync();

    dp = opendir(c_dir_binary); 
    while ((dent = readdir(dp)) != NULL) {
        if (strncmp(dent->d_name, ".", 1) == F_SUCCESS ||
            strncmp(dent->d_name, "..", 2) == F_SUCCESS)
            continue;

        if (!backup_file(dent->d_name)) {
            closedir(dp);
            return;
        }
    }
    closedir(dp);

    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// Modifies: The 'running' global (defs.h) may be set to 0.
//
// Effects: If called by the system object with no arguments, sets
//          'running' to 0, causing the program to exit after this
//          iteration of the main loop finishes.  Returns 1.
//
*/

void func_shutdown(void) {

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    running = 0;
    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// Modifies: heartbeat_freq (defs.h)
//
*/

void func_set_heartbeat(void) {
    data_t *args;

    if (!func_init_1(&args, INTEGER))
        return;

    if (args[0].u.val <= 0)
        args[0].u.val = -1;
    heartbeat_freq = args[0].u.val;
    pop(1);
}

