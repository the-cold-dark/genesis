/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Procedures to handle logging and fatal errors.
*/

#include "defs.h"

#include <sys/types.h>
#include <stdarg.h>
#include "cache.h"
#include "util.h"
#include "sig.h"

void panic(char * s, ...) {
    va_list vargs;
    static Bool panic_state = NO;

    va_start(vargs,s);
    fprintf(errfile, "[%s] %s: ", timestamp(NULL),
            (panic_state ? "RECURSIVE PANIC" : "PANIC"));
    vfprintf(errfile,s,vargs);
    va_end(vargs);
    fputc('\n',errfile);

    if (!panic_state) {
	panic_state = YES;
        fprintf(errfile, "[%s] doing binary dump...", timestamp(NULL));
	cache_sync();
        fputs("Done\n", errfile);
    }

    fprintf(errfile, "[%s] Creating Core Image...", timestamp(NULL));
    dump_core_and_exit();
}

void fail_to_start(char *s) {
    fprintf(errfile, "[%s] FAILED TO START: %s\n", timestamp(NULL), s);

    exit(1);
}

void write_log(char *fmt, ...) {
    va_list arg;
    cStr *str;

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
    cStr *str;

    va_start(arg, fmt);
    str = vformat(fmt, arg);
    va_end(arg);

    fprintf(errfile, "[%s] %s\n", timestamp(NULL), string_chars(str));
    fflush(errfile);

    string_discard(str);
}

