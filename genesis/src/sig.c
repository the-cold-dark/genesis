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

#include "config.h"
#include "defs.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
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
    signal(SIGUSR1, catch_signal);
    signal(SIGUSR2, catch_signal);
    signal(SIGCHLD, catch_SIGCHLD);
}

void catch_SIGCHLD(int sig) {
    /* ah, our kiddie is done, tell it to go away */
    waitpid(-1, NULL, WNOHANG);

    /* some older OS's reset the signal handler, rather annoying */
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
    task(SYSTEM_OBJNUM, signal_id, 2, &arg1, &arg2);

    /* figure out what to do */
    switch(sig) {
        case SIGHUP:
            atomic = 0;
            handle_connection_output();
            flush_files();
        case SIGFPE:
        case SIGUSR2:
            /* let the db do what it wants from here */
            break;
        case SIGUSR1:
            /* USR1 goes back to the main loop */
            longjmp(main_jmp, 1);
            break;
        case SIGILL:
        case SIGSEGV:
            /* lets panic and hopefully shutdown without frobbing the db */
            panic(sig_name(sig));
            break;
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        default: /* shutdown nicely */
            if (running) {
                write_err("*** Attempting normal shutdown ***");
                running = 0;
                longjmp(main_jmp, 1);
            } else {
                panic(sig_name(sig));
            }
            break;
    }
}

