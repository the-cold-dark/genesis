/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define DEFS_C

#include <sys/types.h>
#include <time.h>
#include "defs.h"

#define INIT_VAR(var, name, len) { \
        var = EMALLOC(char, len + 1); \
        strncpy(var, name, len); \
        var[len] = (char) NULL; \
    }

void init_defs(void) {
#ifdef HAVE_TM_ZONE
    struct tm * tms;
    time_t t;
#endif
    c_interactive = NO;
    running = YES;
    atomic = NO;
    heartbeat_freq = 5;

    INIT_VAR(c_dir_binary, "binary", 6);
    INIT_VAR(c_dir_textdump, "textdump", 8);
    INIT_VAR(c_dir_bin, "dbbin", 8);
    INIT_VAR(c_dir_root, "root", 4);
    INIT_VAR(c_logfile, "logs/db.log", 11);
    INIT_VAR(c_errfile, "logs/driver.log", 15);
    INIT_VAR(c_runfile, "logs/genesis.run", 16);

    logfile = stdout;
    errfile = stderr;
    cache_width = CACHE_WIDTH;
    cache_depth = CACHE_DEPTH;

#ifdef HAVE_TM_ZONE
    time(&t);
    tms = localtime(&t);
    str_tzname = string_from_chars(tms->tm_zone, strlen(tms->tm_zone));
#else
# ifdef HAVE_TZNAME 
    str_tzname = string_from_chars(tzname[0], strlen(tzname[0]));
# else 
    str_tzname = string_new(0);
# endif
#endif

    str_hostname = string_new(0);
}

#undef INIT_VAR

#undef _defs_
