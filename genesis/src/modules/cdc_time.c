/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_time.c
// ---
// Miscellaneous operations.
*/

#include "config.h"

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>    /* for mtime() */
#include "defs.h"
#include "y.tab.h"
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"
#include "util.h"

#if defined(sys_ultrix4_4)
int gettimeofday (struct timeval *tp, struct timezone *tzp);
#endif

void op_time(void) {
    /* Take no arguments. */
    if (!func_init_0())
	return;

    push_int(time(NULL));
}

void op_localtime(void) {
    struct tm * tms;
    data_t * d;
    list_t * l;
    time_t t;
    register int x;

    if (!func_init_0())
	return;

    time(&t);
    tms = localtime(&t);

#define __LSIZE__ 10

    l = list_new(__LSIZE__);
    d = list_empty_spaces(l, __LSIZE__);
    for (x=0; x < __LSIZE__; x++)
        d[x].type = INTEGER;

    d[0].u.val = (int) t;
    d[1].u.val = tms->tm_sec;
    d[2].u.val = tms->tm_min;
    d[3].u.val = tms->tm_hour;
    d[4].u.val = tms->tm_mday;
    d[5].u.val = tms->tm_mon;
    d[6].u.val = tms->tm_year;
    d[7].u.val = tms->tm_wday;
    d[8].u.val = tms->tm_yday;
    d[9].u.val = tms->tm_isdst;

#undef __LSIZE__

    push_list(l);
    list_discard(l);
}

/* May as well give it to them, the code is in the driver */
void op_timestamp(void) { string_t * str;
    char     * s;

    /* Take no arguments. */
    if (!func_init_0())
        return;

    s = timestamp(NULL);
    str = string_from_chars(s, strlen(s));

    push_string(str);
    string_discard(str);
}

void op_strftime(void) {
    char        s[LINE];
    char      * fmt;
    data_t    * args;
    string_t  * str;
    int         nargs;
    time_t      tt;
    struct tm * t;

    if (!func_init_1_or_2(&args, &nargs, STRING, INTEGER))
	return;

    tt = ((nargs == 2) ? (time_t) args[1].u.val : time(NULL));
    t  = localtime(&tt);
 
    fmt = string_chars(args[0].u.str);

    /* some OS's are weird and do odd things when you end in %
       (accidentally or no) */
    if (fmt[strlen(fmt)] == '%')
        fmt[strlen(fmt)] = NULL;

    if (strftime(s, LINE, fmt, t) == (size_t) 0) {
        cthrow(range_id,
               "Format results in a string longer than 80 characters.");
        return;
    }

    str = string_from_chars(s, strlen(s));

    pop(nargs);
    push_string(str);
    string_discard(str);
}

void op_mtime(void) {
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tp;
#endif

    /* Take no prisoners (bwahaha) */
    if (!func_init_0())
        return;

#ifdef HAVE_GETTIMEOFDAY
    /* usec is microseconds */
    gettimeofday(&tp, NULL);

    push_int((int) tp.tv_usec);
#else
    push_int(-1);
#endif
}

void op_ctime(void) {
    data_t *args;
    int num_args;
    time_t tval;
    char *timestr;
    string_t *str;

    /* Take an optional integer argument. */
    if (!func_init_0_or_1(&args, &num_args, INTEGER))
	return;

    tval = (num_args) ? args[0].u.val : time(NULL);
    timestr = ctime(&tval);
    str = string_from_chars(timestr, 24);

    pop(num_args);
    push_string(str);
    string_discard(str);
}

