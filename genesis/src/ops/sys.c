/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#ifdef __UNIX__
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>  /* directory funcs */
#ifdef __MSVC__
#include <direct.h>
#endif
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
    strcpy(dest, c_dir_binary);
    strcat(dest, ".bak/");
    strcat(dest, file);

    from_fd = open(source, O_RDONLY | O_BINARY, 0);
    if (from_fd == F_FAILURE)
        x_THROW(source)

#ifdef __MSVC__
    to_fd = open(dest, (O_WRONLY|O_TRUNC|O_CREAT|O_BINARY), (_S_IREAD|_S_IWRITE));
#else
    to_fd = open(dest, (O_WRONLY|O_TRUNC|O_CREAT|O_BINARY), (S_IRUSR|S_IWUSR));
#endif
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
    char            buf[BUF];
    struct stat     statbuf;
    struct dirent * dent;
    DIR           * dp;

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    if (dump_db_file)
        THROW((perm_id, "A dump is already in progress!"))

    /* get binary.bak, make sure its ours */
    strcpy(buf, c_dir_binary);
    strcat(buf, ".bak");
    if (stat(buf, &statbuf) == F_FAILURE) {
#ifdef __MSVC__
        if (mkdir(buf) == F_FAILURE)
#else
        if (mkdir(buf, READ_WRITE_EXECUTE) == F_FAILURE)
#endif
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(GETERR())))
    } else if (!S_ISDIR(statbuf.st_mode)) {
        if (unlink(buf) == F_FAILURE)
            THROW((file_id, "Cannot delete file \"%s\": %s", buf, strerror(GETERR())))
#ifdef __MSVC__
        if (mkdir(buf) == F_FAILURE)
#else
        if (mkdir(buf, READ_WRITE_EXECUTE) == F_FAILURE)
#endif
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(GETERR())))
    }

    /* sync the db */
    cache_sync();

    /* copy the index files and '.clean' */
    dp = opendir(c_dir_binary); 
    /* if this failed, then this backup can't complete. die. */
    if (dp == NULL) {
        write_err("ERROR: error in backup: %s: %s", c_dir_binary, strerror(GETERR()));
        cthrow(file_id, "%s: %s", c_dir_binary, strerror(GETERR()));
        return;
    }
    while ((dent = readdir(dp)) != NULL) {
        if (*(dent->d_name) == '.' || !strncmp(dent->d_name, "objects", 7))
            continue;

        if (!backup_file(dent->d_name)) {
            closedir(dp);
            return;
        }
    }
    closedir(dp);

    /* start asynchronous backup of the object db file */
    strcat(buf, "/objects");
    if (db_start_dump(buf))
        THROW((file_id, "Unable to open dump db file \"%s\"", buf))

    /* return '1' */
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

COLDC_FUNC(set_heartbeat) {
    cData *args;

    if (!func_init_1(&args, INTEGER))
        return;

    if (INT1 <= 0)
        INT1 = -1;
    heartbeat_freq = INT1;

    /* don't push or pop--let the arg be the return value */
}

/*
// -----------------------------------------------------------------

 * Datasize limit (example: 1024K)
 * Task forking limit (for instance, 20. Each time a task forks, each 
   child gets only a half of this quota)
 * Recursion depth (good default: 128)
 * Object swapping (a task can't swap an object from disk-db more than 
   (for example) 16 times before calling pause() and preempting)
   (we might be able to do away with swap limit with Genesis 2.0, as
   we'll go multithreaded then, and it'll be much harder to lag the server)

*/
COLDC_FUNC(config) {
    cData * args;
    Int     argc,
            rval;

    /* change to ANY and adjust appropriately below, if we start accepting
       non-integers */
    if (!func_init_1_or_2(&args, &argc, SYMBOL, INTEGER))
        return;

    if (argc == 1) {
        if (SYM1 == datasize_id)
            rval = limit_datasize;
        else if (SYM1 == forkdepth_id)
            rval = limit_fork;
        else if (SYM1 == calldepth_id)
            rval = limit_calldepth;
        else if (SYM1 == recursion_id)
            rval = limit_recursion;
        else if (SYM1 == objswap_id)
            rval = limit_objswap;
        else
            THROW((type_id, "Invalid configuration name."))
    } else {
        if (SYM1 == datasize_id)
            rval = limit_datasize = INT2;
        else if (SYM1 == forkdepth_id)
            rval = limit_fork = INT2;
        else if (SYM1 == calldepth_id)
            rval = limit_calldepth = INT2;
        else if (SYM1 == recursion_id)
            rval = limit_recursion = INT2;
        else if (SYM1 == objswap_id)
            rval = limit_objswap = INT2;
        else
            THROW((type_id, "Invalid configuration name."))
    }

    pop(argc);
    push_int(rval);
}

COLDC_FUNC(cache_info) {
    cList * list;

    if (!func_init_0())
        return;

    list = cache_info(0);

/*    pop(1); */
    push_list(list);
    list_discard(list);
}

