/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
*/

#include "config.h"
#include "defs.h"

#include "memory.h"
#include "cache.h"
#include "log.h"       /* op_log() */
#include "execute.h"

void func_log(void) {
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

#define SHELL_FAILURE 127

void func_backup(void) {
    char          buf1[255];
    char          buf2[255];
    struct stat   statbuf;

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    strcpy(buf1, c_dir_binary);
    strcat(buf1, ".bak");

    /* blast old backups */
    if (stat(buf1, &statbuf) != F_FAILURE) {
        sprintf(buf2, "rm -rf %s", buf1);
        if (system(buf2) == SHELL_FAILURE)
            push_int(0);
    }
    if (stat(buf1, &statbuf) != F_FAILURE) {
        cthrow(error_id, "Unable to remove existing directory %s\n", buf1);
        return;
    }
    cache_sync();
    sprintf(buf2, "cp -r %s %s", c_dir_binary, buf1);
    if (system(buf2) != SHELL_FAILURE) {
        push_int(1);
    } else {
        push_int(-1);
    }
}

#undef SHELL_FAILURE

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
