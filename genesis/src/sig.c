/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: sig.c
// ---
// Coldmud signal handling.
*/

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "config.h"
#include "defs.h"
#include "sig.h"
#include "log.h"          /* write_err() */
#include "execute.h"      /* task() */
#include "cdc_string.h"   /* string_from_chars() */
#include "data.h"
#include "y.tab.h"

void catch_SIGCHLD(int sig);

void uninit_sig(void) {
    signal(SIGFPE,  SIG_DFL);
    signal(SIGILL,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGINT,  SIG_DFL);
    signal(SIGHUP,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    signal(SIGUSR2, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

void init_sig(void) {
    signal(SIGFPE,  catch_signal);
    signal(SIGILL,  catch_signal);
    signal(SIGQUIT, catch_signal);
    signal(SIGINT,  catch_signal);
    signal(SIGHUP,  catch_signal);
    signal(SIGTERM, catch_signal);
    signal(SIGALRM, catch_signal);
    signal(SIGUSR1, catch_signal);
    signal(SIGUSR2, catch_signal);
    signal(SIGCHLD, catch_SIGCHLD);
}

void catch_SIGCHLD(int sig) {
    waitpid(-1, NULL, WNOHANG);
    signal(SIGCHLD, catch_SIGCHLD);
}

char *sig_name(int sig) {
    switch(sig) {
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGQUIT: return "SIGQUIT";
        case SIGSEGV: return "SIGSEGV";
        case SIGINT:  return "SIGINT";
        case SIGHUP:  return "SIGHUP";
        case SIGTERM: return "SIGTERM";
        case SIGALRM: return "SIGALRM";
        case SIGUSR1: return "SIGUSR1";
        case SIGUSR2: return "SIGUSR2";
        default:      return "Unknown";
    }
    return NULL;
}

/* void catch_signal(int sig, int code, struct sigcontext *scp) { */
void catch_signal(int sig) {
    char *sptr;
    string_t *sigstr;
    data_t arg1, arg2;

    signal(sig, catch_signal);

    sptr = sig_name(sig);
    sigstr = string_from_chars(sptr, strlen(sptr));

    write_err("Caught signal %d: %S", sig, sigstr);

    /* only pass onto the db if we are 'executing' */
    if (!running)
        return;

    /* send a message to the system object */
    arg1.type = SYMBOL;
    arg1.u.symbol = ident_get(sig_name(sig));
    arg2.type = STRING;
    arg2.u.str = sigstr;
    task(NULL, SYSTEM_DBREF, signal_id, 2, &arg1, &arg2);

    /* figure out what to do */
    switch(sig) {
        case SIGFPE:
        case SIGUSR1:
        case SIGUSR2:
        case SIGHUP:
        case SIGALRM:
            /* let the server do what it wants, don't shutdown */
            break;
        case SIGILL:
        case SIGSEGV:
            /* lets panic and hopefully dump */
            panic(sig_name(sig));
            break;
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        default: /* shutdown nicely */
            if (running) {
                write_err("*** Attempting normal shutdown ***");
                running = 0;
            } else {
                panic(sig_name(sig));
            }
            break;
    }
}

