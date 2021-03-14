/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include <sys/types.h>
#include <time.h>
#include "defs.h"

jmp_buf main_jmp;

char * c_dir_binary;
char * c_dir_textdump;
char * c_dir_bin;
char * c_dir_root;
char * c_logfile;
char * c_errfile;
char * c_runfile;

FILE * logfile;
FILE * errfile;
cStr * str_tzname;
cStr * str_hostname;
cStr * str_release;
cStr * str_system;

bool coldcc;
bool running;
bool atomic;
Int  heartbeat_freq;

Int cache_width;
Int cache_depth;
#ifdef USE_CLEANER_THREAD
Int  cleaner_wait;
cDict * cleaner_ignore_dict;
#endif

void init_defs(void);
void uninit_defs(void);

/* limits configurable with 'config()' */
Int  limit_datasize;
Int  limit_fork;
Int  limit_calldepth;
Int  limit_recursion;
Int  limit_objswap;

/* driver config parameters accessible through config() */
Int  cache_log_flag;
Int  cache_watch_count;
cObjnum cache_watch_object;
Int  log_malloc_size;
Int  log_method_cache;

#ifdef USE_CACHE_HISTORY
/* cache stats stuff */
Int cache_history_size;
#endif

#define INIT_VAR(var, name, len) { \
        var = EMALLOC(char, len + 1); \
        memcpy(var, name, len); \
        var[len] = '\0'; \
    }

void init_defs(void) {
#ifdef HAVE_TM_ZONE
    struct tm * tms;
    time_t t;
#endif
    coldcc = false;
    running = true;
    atomic = false;
    heartbeat_freq = 5;
    cache_search = START_SEARCH_AT;

    INIT_VAR(c_dir_binary, "binary", 6);
    INIT_VAR(c_dir_textdump, "textdump", 8);
    INIT_VAR(c_dir_bin, "dbbin", 5);
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
    str_tzname = string_from_chars((char*)tms->tm_zone, strlen(tms->tm_zone));
#else
# ifdef HAVE_TZNAME
    str_tzname = string_from_chars(tzname[0], strlen(tzname[0]));
# else
    str_tzname = string_new(0);
# endif
#endif

    str_hostname = string_new(0);
    str_release = string_from_chars(VERSION_RELEASE, strlen(VERSION_RELEASE));
    str_system = string_from_chars(SYSTEM_TYPE, strlen(SYSTEM_TYPE));

    log_malloc_size = 0;
    log_method_cache = 0;

#ifdef USE_CACHE_HISTORY
    ancestor_cache_history = list_new(0);
    method_cache_history = list_new(0);

    cache_history_size = 50;
#endif
}

#undef INIT_VAR

void uninit_defs(void) {
    efree(c_dir_binary);
    efree(c_dir_textdump);
    efree(c_dir_bin);
    efree(c_dir_root);
    efree(c_logfile);
    efree(c_errfile);

    string_discard(str_tzname);
    string_discard(str_hostname);
    string_discard(str_release);
    string_discard(str_system);

#ifdef USE_CACHE_HISTORY
    list_discard(ancestor_cache_history);
    list_discard(method_cache_history);
#endif
}
