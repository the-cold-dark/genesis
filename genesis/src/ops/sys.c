/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>  /* directory funcs */
#include "cache.h"
#include "execute.h"
#include "binarydb.h"

void func_dblog(void) {
    cData * args;

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
        cthrow(file_id, "%s: %s", _file_, strerror(GETERR())); \
        return 0; \
    }

INTERNAL Bool backup_file(char * file) {
    static char buf[MAXBSIZE];
    Bool rval = TRUE;
    int from_fd, rcount, to_fd, wcount;
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
            rval = FALSE;
            break;
        } else if (wcount == F_FAILURE) {
            cthrow(file_id, "%s", strerror(GETERR()));
            rval = FALSE;
            break;
        }
    }

    if (rcount == F_FAILURE && rval) {
        cthrow(file_id, "%s", strerror(GETERR()));
        rval = FALSE;
    }

    (void)close(from_fd);
    if (close(to_fd) && rval) {
        cthrow(file_id, "%s", strerror(GETERR()));
        rval = FALSE;
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
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(GETERR())))
    } else if (!S_ISDIR(statbuf.st_mode)) {
        if (unlink(buf) == F_FAILURE)
            THROW((file_id, "Cannot delete file \"%s\": %s", buf, strerror(GETERR())))
        if (mkdir(buf, READ_WRITE_EXECUTE) == F_FAILURE)
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(GETERR())))
    }

    /* sync the db */
    cache_sync();

    dp = opendir(c_dir_binary); 
    while ((dent = readdir(dp)) != NULL) {
        if (strncmp(dent->d_name, ".", 1) == F_SUCCESS &&
            strcmp(dent->d_name, ".clean")) /* true == failed to match */
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

    running = NO;
    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// Modifies: heartbeat_freq (defs.h)
//
*/

void func_set_heartbeat(void) {
    cData *args;

    if (!func_init_1(&args, INTEGER))
        return;

    if (args[0].u.val <= 0)
        args[0].u.val = -1;
    heartbeat_freq = args[0].u.val;
    pop(1);
}

