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

#define NATIVE_MODULE

#include "config.h"
#include "defs.h"

#include <time.h>
#include <sys/time.h>    /* for mtime(), getrusage() */
#include <sys/resource.h>      /* getrusage()  25-Jan-95 BJG */
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"
#include "util.h"
#include "net.h"

NATIVE_METHOD(strftime) {
    char        s[LINE];
    char      * fmt;
    time_t      tt;
    struct tm * t;

    INIT_1_OR_2_ARGS(STRING, INTEGER);

    tt = ((argc == 2) ? (time_t) INT2 : time(NULL));
    t  = localtime(&tt);
 
    fmt = string_chars(STR1);

    /* some OS's are weird and do odd things when you end in %
       (accidentally or no) */
    if (fmt[strlen(fmt)] == '%')
        fmt[strlen(fmt)] = NULL;

    if (strftime(s, LINE, fmt, t) == (size_t) 0)
       THROW((range_id,"Format results in a string longer than 80 characters."))

    CLEAN_RETURN_STRING(string_from_chars(s, strlen(s)));
}

NATIVE_METHOD(next_objnum) {
    INIT_NO_ARGS();

    CLEAN_RETURN_OBJNUM(db_top);
}

#ifdef HAVE_GETRUSAGE
#if defined(sys_solaris) || defined(sys_ultrix)
extern int getrusage(int, struct rusage *);
#endif
#endif

NATIVE_METHOD(status) {
#ifdef HAVE_GETRUSAGE
    struct rusage r;
#endif
    list_t *status;
    data_t *d;
    int x;

    INIT_NO_ARGS();

#define __LLENGTH__ 19

    status = list_new(__LLENGTH__);
    d = list_empty_spaces(status, __LLENGTH__);
    for (x=0; x < __LLENGTH__; x++)
        d[x].type = INTEGER;

#ifndef HAVE_GETRUSAGE

    for (x=0; x < __LLENGTH__ - 1; x++)
        d[x].u.val = -1;

#else

    getrusage(RUSAGE_SELF, &r);
    d[0].u.val = (int) r.ru_utime.tv_sec; /* user time used (seconds) */
    d[1].u.val = (int) r.ru_utime.tv_usec;/* user time used (microseconds) */
    d[2].u.val = (int) r.ru_stime.tv_sec; /* system time used (seconds) */
    d[3].u.val = (int) r.ru_stime.tv_usec;/* system time used (microseconds) */
    d[4].u.val = (int) r.ru_maxrss;
    d[7].u.val = (int) r.ru_idrss;       /* integral unshared data size */
    d[8].u.val = (int) r.ru_minflt;      /* page reclaims */
    d[9].u.val = (int) r.ru_majflt;      /* page faults */
    d[10].u.val = (int) r.ru_nswap;       /* swaps */
    d[11].u.val = (int) r.ru_inblock;     /* block input operations */
    d[12].u.val = (int) r.ru_oublock;     /* block output operations */
    d[13].u.val = (int) r.ru_msgsnd;      /* messages sent */
    d[14].u.val = (int) r.ru_msgrcv;      /* messages received */
    d[15].u.val = (int) r.ru_nsignals;    /* signals received */
    d[16].u.val = (int) r.ru_nvcsw;       /* voluntary context switches */
    d[17].u.val = (int) r.ru_nivcsw;      /* involuntary context switches */

#endif

    d[18].u.val = (int) atomic;
#undef __LLENGTH__

    CLEAN_RETURN_LIST(status);
}

NATIVE_METHOD(version) {
    list_t *version;
    data_t *d;

    INIT_NO_ARGS();

    /* Construct a list of the version numbers and push it. */
    version = list_new(3);
    d = list_empty_spaces(version, 3);
    d[0].type = d[1].type = d[2].type = INTEGER;
    d[0].u.val = VERSION_MAJOR;
    d[1].u.val = VERSION_MINOR;
    d[2].u.val = VERSION_PATCH;

    CLEAN_RETURN_LIST(version);
}

/*
// -----------------------------------------------------------------
*/
NATIVE_METHOD(hostname) {
    string_t * name;

    INIT_1_ARG(STRING);

    name = hostname(string_chars(STR1));

    CLEAN_RETURN_STRING(name);
}

/*
// -----------------------------------------------------------------
*/
NATIVE_METHOD(ip) {
    string_t * sip;

    INIT_1_ARG(STRING);

    sip = ip(string_chars(STR1));

    CLEAN_RETURN_STRING(sip);
}

