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

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include "config.h"
#include "defs.h"
#include "log.h"
#include "dump.h"
#include "cdc_types.h"
#include "util.h"

void panic(char *s) {
    static int panic_state = 0;

    fprintf(stderr, "[%s] %s: %s\n", timestamp(NULL),
            (panic_state ? "RECURSIVE PANIC" : "PANIC"), s);

    if (!panic_state) {
	panic_state = 1;
        fprintf(stderr, "[%s] doing binary dump...", timestamp(NULL));
	binary_dump();
        fputs("Done\n", stderr);
    }
    exit(1);
}

void abort(void) {
  panic("Aborted");
  exit(1);  /* Never reached.  Avoids warnings on some compilers, tho */
}

void fail_to_start(char *s) {
    fprintf(stderr, "[%s] FAILED TO START: %s\n", timestamp(NULL), s);

    exit(1);
}

void write_log(char *fmt, ...) {
    va_list arg;
    string_t *str;

    va_start(arg, fmt);

    str = vformat(fmt, arg);

    fputs(string_chars(str), stdout);
    fputc('\n', stdout);

    fflush(stdout);

    string_discard(str);
    va_end(arg);
}

void write_err(char *fmt, ...) {
    va_list arg;
    string_t *str;

    va_start(arg, fmt);
    str = vformat(fmt, arg);
    va_end(arg);

    fprintf(stderr, "[%s] %s\n", timestamp(NULL), string_chars(str));
    fflush(stderr);

    string_discard(str);
}

