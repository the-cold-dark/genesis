/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: log.c
// ---
// Procedures to handle logging and fatal errors.
*/

#include "config.h"
#include "defs.h"

#include <sys/types.h>
#include <stdarg.h>
#include "log.h"
#include "cache.h"
#include "cdc_types.h"
#include "util.h"

void panic(char *s, ...) {
           va_list vargs;
    static int     panic_state = 0;

    va_start(vargs,s);
    fprintf(errfile, "[%s] %s: ", timestamp(NULL),
            (panic_state ? "RECURSIVE PANIC" : "PANIC"));
    vfprintf(errfile,s,vargs);
    va_end(vargs);
    fputc('\n',errfile);

    if (!panic_state) {
	panic_state = 1;
        fprintf(errfile, "[%s] doing binary dump...", timestamp(NULL));
	cache_sync();
        fputs("Done\n", errfile);
    }

    exit(1);
}

void abort(void) {
  panic("Aborted");
  exit(1);  /* Never reached.  Avoids warnings on some compilers, tho */
}

void fail_to_start(char *s) {
    fprintf(errfile, "[%s] FAILED TO START: %s\n", timestamp(NULL), s);

    exit(1);
}

void write_log(char *fmt, ...) {
    va_list arg;
    string_t *str;

    va_start(arg, fmt);

    str = vformat(fmt, arg);

    fputs(string_chars(str), logfile);
    fputc('\n', logfile);

    fflush(logfile);

    string_discard(str);
    va_end(arg);
}

void write_err(char *fmt, ...) {
    va_list arg;
    string_t *str;

    va_start(arg, fmt);
    str = vformat(fmt, arg);
    va_end(arg);

    fprintf(errfile, "[%s] %s\n", timestamp(NULL), string_chars(str));
    fflush(errfile);

    string_discard(str);
}

