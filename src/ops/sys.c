/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include "functions.h"
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

COLDC_FUNC(dblog) {
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
// Effects: If called by the system object with no arguments,
//          performs a binary dump, ensuring that the files db and
//          db are consistent.  Returns 1 if the binary dump
//          succeeds, or 0 if it fails.
//
*/

#ifndef MAXBSIZE
#define MAXBSIZE 16384
#endif

#define x_THROW(_file_) do { \
        cthrow(file_id, "%s: %s", _file_, strerror(GETERR())); \
        return 0; \
    } while(0)

static Bool backup_file(char * file) {
    static char buf[MAXBSIZE];
    Bool rval = true;
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
        x_THROW(source);

#ifdef __MSVC__
    to_fd = open(dest, (O_WRONLY|O_TRUNC|O_CREAT|O_BINARY), (_S_IREAD|_S_IWRITE));
#else
    to_fd = open(dest, (O_WRONLY|O_TRUNC|O_CREAT|O_BINARY), (S_IRUSR|S_IWUSR));
#endif
    if (to_fd == F_FAILURE)
        x_THROW(dest);

    while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
        wcount = write(to_fd, buf, rcount);
        if (rcount != wcount) {
            cthrow(file_id, "Error on copying %s to %s", source, dest);
            rval = false;
            break;
        } else if (wcount == F_FAILURE) {
            cthrow(file_id, "%s", strerror(GETERR()));
            rval = false;
            break;
        }
    }

    if (rcount == F_FAILURE && rval) {
        cthrow(file_id, "%s", strerror(GETERR()));
        rval = false;
    }

    (void)close(from_fd);
    if (close(to_fd) && rval) {
        cthrow(file_id, "%s", strerror(GETERR()));
        rval = false;
    }

    return rval;
}

COLDC_FUNC(sync) {
    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* sync the db */
    cache_sync();

    /* return '1' */
    push_int(1);
}

COLDC_FUNC(backup) {
    char            buf[BUF];
    struct stat     statbuf;
    struct dirent * dent;
    DIR           * dp;

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    if (dump_db_file)
        THROW((perm_id, "A dump is already in progress!"));

    /* get binary.bak, make sure its ours */
    strcpy(buf, c_dir_binary);
    strcat(buf, ".bak");
    if (stat(buf, &statbuf) == F_FAILURE) {
#ifdef __MSVC__
        if (mkdir(buf) == F_FAILURE)
#else
        if (mkdir(buf, READ_WRITE_EXECUTE) == F_FAILURE)
#endif
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(GETERR())));
    } else if (!S_ISDIR(statbuf.st_mode)) {
        if (unlink(buf) == F_FAILURE)
            THROW((file_id, "Cannot delete file \"%s\": %s", buf, strerror(GETERR())));
#ifdef __MSVC__
        if (mkdir(buf) == F_FAILURE)
#else
        if (mkdir(buf, READ_WRITE_EXECUTE) == F_FAILURE)
#endif
            THROW((file_id, "Cannot create directory \"%s\": %s", buf, strerror(GETERR())));
    }

    /* sync the db */
    cache_sync();

#ifdef USE_CLEANER_THREAD
#ifdef DEBUG_CLEANER
    write_err("func_backup: locked cleaner");
#endif
    pthread_mutex_lock(&cleaner_lock);
#endif

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
    if (simble_dump_start(buf))
        THROW((file_id, "Unable to open dump db file \"%s\"", buf));

#ifdef USE_CLEANER_THREAD
    pthread_mutex_unlock(&cleaner_lock);
#ifdef DEBUG_CLEANER
    write_err("func_backup: unlocked cleaner");
#endif
#endif

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

COLDC_FUNC(shutdown) {

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    running = false;
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

#define _CONFIG_INT(id, var) \
        if (SYM1 == id) { \
            if (argc == 2) { \
                if (args[ARG2].type != INTEGER) \
                    THROW((type_id, "Expected an integer")); \
                var = INT2; \
            } \
            pop(argc); \
            push_int(var); \
            return; \
        }

#define _CONFIG_CLEANERWAIT(id, var) \
        if (SYM1 == id) { \
            if (argc == 2) { \
                if (args[ARG2].type != INTEGER) \
                    THROW((type_id, "Expected an integer")); \
                var = INT2; \
                pthread_cond_signal(&cleaner_condition); \
            } \
            pop(argc); \
            push_int(var); \
            return; \
        }

#define _CONFIG_OBJNUM(id, var) \
        if (SYM1 == id) { \
            if (argc == 2) { \
                if (args[ARG2].type != OBJNUM) \
                    THROW((type_id, "Expected an $object")); \
                var = OBJNUM2; \
            } \
            pop(argc); \
            push_objnum(var); \
            return; \
        }

#define _CONFIG_DICT(id, var) \
        if (SYM1 == id) { \
            if (argc == 2) { \
                if (args[ARG2].type != DICT) \
                    THROW((type_id, "Expected a dict")); \
                dict_discard(var); \
                var = dict_dup(DICT2); \
            } \
            pop(argc); \
            push_dict(var); \
            return; \
        }

COLDC_FUNC(config) {
    cData * args;
    Int     argc;

    /* change to ANY and adjust appropriately below, if we start accepting
       non-integers */
    if (!func_init_1_or_2(&args, &argc, SYMBOL, ANY_TYPE))
        return;

    _CONFIG_INT(datasize_id,                   limit_datasize)
    _CONFIG_INT(forkdepth_id,                  limit_fork)
    _CONFIG_INT(calldepth_id,                  limit_calldepth)
    _CONFIG_INT(recursion_id,                  limit_recursion)
    _CONFIG_INT(objswap_id,                    limit_objswap)
    _CONFIG_INT(cachelog_id,                   cache_log_flag)
    _CONFIG_INT(cachewatchcount_id,            cache_watch_count)
    _CONFIG_OBJNUM(cachewatch_id,              cache_watch_object)
#ifdef USE_CLEANER_THREAD
    _CONFIG_CLEANERWAIT(cleanerwait_id,        cleaner_wait)
    _CONFIG_DICT(cleanerignore_id,             cleaner_ignore_dict)
#endif
    _CONFIG_INT(log_malloc_size_id,            log_malloc_size)
    _CONFIG_INT(log_method_cache_id,           log_method_cache)
#ifdef USE_CACHE_HISTORY
    _CONFIG_INT(cache_history_size_id,         cache_history_size)
#endif
    THROW((type_id, "Invalid configuration name."));
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

COLDC_FUNC(cache_stats) {
    cData * args;
    cList * list, * entry;
    cData * val, list_entry;

    if (!func_init_1(&args, SYMBOL))
        return;

    if (SYM1 == ancestor_cache_id) {
#ifdef USE_CACHE_HISTORY
        list = list_dup(ancestor_cache_history);
        entry = ancestor_cache_info();
#else
        list = list_new(0);
        entry = list_new(0);
#endif
        list_entry.type = LIST;
        list_entry.u.list = entry;
        list = list_add(list, &list_entry);
        list_discard(entry);
    } else if (SYM1 == method_cache_id) {
#ifdef USE_CACHE_HISTORY
        list = list_dup(method_cache_history);
        entry = method_cache_info();
#else
        list = list_new(0);
        entry = list_new(0);
#endif
        list_entry.type = LIST;
        list_entry.u.list = entry;
        list = list_add(list, &list_entry);
        list_discard(entry);
    } else if (SYM1 == name_cache_id) {
        list = list_new(2);
        val = list_empty_spaces(list, 2);
        val[0].type = INTEGER;
        val[0].u.val = name_cache_hits;
        val[1].type = INTEGER;
        val[1].u.val = name_cache_misses;
    } else if (SYM1 == object_cache_id) {
        THROW((type_id, "Object cache stats not yet supported."));
    } else {
        THROW((type_id, "Invalid cache type."));
    }

    pop(1);
    push_list(list);
    list_discard(list);
}
