/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define NATIVE_MODULE

#include "cdc.h"

#include "cdc_db.h"
#include <time.h>
#ifdef __UNIX__
#include <sys/time.h>    /* for mtime(), getrusage() */
#include <sys/resource.h>      /* getrusage()  25-Jan-95 BJG */
#endif
#include "util.h"
#include "net.h"
#include "dns.h"

NATIVE_METHOD(strftime) {
    char        s[LINE];
    char      * fmt;
    time_t      tt;
    struct tm * t;

    INIT_1_OR_2_ARGS(STRING, INTEGER);

    if (STR1->len == 0) {
        s[0] = '\0';
        CLEAN_RETURN_STRING(string_from_chars(s, 0));
    }

#ifdef __BORLANDC__
    if (argc == 2) {
        if (INT2 < 18000) {
            THROW((type_id,
         "Borland's time util is broken, and requires time values above 18000"))
        }
        tt = (time_t) INT2;
    } else {
        tt = time(NULL);
    }
#else
    tt = ((argc == 2) ? (time_t) INT2 : time(NULL));
#endif
    t  = localtime(&tt);
 
    fmt = string_chars(STR1);

    /* some OS's are weird and do odd things when you end in %
       (accidentally or no) */
    if (fmt[strlen(fmt)] == '%')
        fmt[strlen(fmt)] = (char) NULL;

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
    cList *status;
    cData *d;
    Int x;

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
    d[0].u.val = (cNum) r.ru_utime.tv_sec; /* user time used (seconds) */
    d[1].u.val = (cNum) r.ru_utime.tv_usec;/* user time used (microseconds) */
    d[2].u.val = (cNum) r.ru_stime.tv_sec; /* system time used (seconds) */
    d[3].u.val = (cNum) r.ru_stime.tv_usec;/* system time used (microseconds) */
    d[4].u.val = (cNum) r.ru_maxrss;
    d[5].u.val = (cNum) 0;
    d[6].u.val = (cNum) 0;
    d[7].u.val = (cNum) r.ru_idrss;       /* integral unshared data size */
    d[8].u.val = (cNum) r.ru_minflt;      /* page reclaims */
    d[9].u.val = (cNum) r.ru_majflt;      /* page faults */
    d[10].u.val = (cNum) r.ru_nswap;       /* swaps */
    d[11].u.val = (cNum) r.ru_inblock;     /* block input operations */
    d[12].u.val = (cNum) r.ru_oublock;     /* block output operations */
    d[13].u.val = (cNum) r.ru_msgsnd;      /* messages sent */
    d[14].u.val = (cNum) r.ru_msgrcv;      /* messages received */
    d[15].u.val = (cNum) r.ru_nsignals;    /* signals received */
    d[16].u.val = (cNum) r.ru_nvcsw;       /* voluntary context switches */
    d[17].u.val = (cNum) r.ru_nivcsw;      /* involuntary context switches */

#endif

    d[18].u.val = (cNum) atomic;
#undef __LLENGTH__

    CLEAN_RETURN_LIST(status);
}

NATIVE_METHOD(version) {
    cList *version;
    cData *d;

    INIT_NO_ARGS();

    /* Construct a list of the version numbers and push it. */
    version = list_new(5);
    d = list_empty_spaces(version, 5);
    d[0].type = d[1].type = d[2].type = INTEGER;
    d[0].u.val = VERSION_MAJOR;
    d[1].u.val = VERSION_MINOR;
    d[2].u.val = VERSION_PATCH;
    d[3].type = STRING;
    d[3].u.str = string_dup(str_release);
    d[4].type = STRING;
    d[4].u.str = string_dup(str_system);

    CLEAN_RETURN_LIST(version);
}

/*
// -----------------------------------------------------------------
*/
NATIVE_METHOD(hostname) {
    cStr * name;
    char   buf[DNS_MAXLEN+1];

    INIT_0_OR_1_ARGS(STRING);

    if (!argc) {
        name = string_dup(str_hostname);
    } else {
        switch (lookup_name_by_ip(string_chars(STR1), buf)) {
            case DNS_INVADDR:
                THROW((address_id, "Invalid IP Address: %S", STR1))
            case DNS_NORESOLV:
                THROW((failed_id, "No name for IP Address %S", STR1))
            case DNS_OVERFLOW:
                THROW((range_id, "DNS Response overflows DNS_MAXLEN!"))
        }
        name = string_from_chars(buf, strlen(buf));
    }

    CLEAN_RETURN_STRING(name);
}

/*
// -----------------------------------------------------------------
*/
NATIVE_METHOD(ip) {
    cStr * sip;
    char   buf[DNS_MAXLEN+1];
    char * p;

    INIT_0_OR_1_ARGS(STRING);

    if (!argc)
        p = string_chars(str_hostname);
    else
        p = string_chars(STR1);

    switch (lookup_ip_by_name(p, buf)) {
        case DNS_NORESOLV:
            THROW((failed_id, "Address %s does not resolv", p))
        case DNS_OVERFLOW:
            THROW((range_id, "DNS Response overflows DNS_MAXLEN!"))
    }
    sip = string_from_chars(buf, strlen(buf));

    CLEAN_RETURN_STRING(sip);
}

