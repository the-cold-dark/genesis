/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <signal.h>
#include <sys/types.h>
#ifdef __UNIX__
#include <sys/wait.h>
#endif
#include "execute.h"      /* vm_task() */
#include "sig.h"

short caught_fpe;

void catch_SIGFPE(int sig);
#ifdef __UNIX__
void catch_SIGCHLD(int sig);
void catch_SIGPIPE(int sig);
#endif

static void uninit_sig(void) {
    signal(SIGFPE,  SIG_DFL);
    signal(SIGILL,  SIG_DFL);
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);
#if !defined(USING_GLIBC_2_0) && !defined(__MSVC__)
    signal(SIGUSR1, SIG_DFL);
    signal(SIGUSR2, SIG_DFL);
#endif
#ifdef __UNIX__
    signal(SIGQUIT, SIG_DFL);
    signal(SIGHUP,  SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
#endif
}

void init_sig(void) {
    caught_fpe = 0;
    signal(SIGFPE,  catch_SIGFPE);
    signal(SIGILL,  catch_signal);
    signal(SIGINT,  catch_signal);
    signal(SIGTERM, catch_signal);
#if !defined(USING_GLIBC_2_0) && !defined(__MSVC__)
    signal(SIGUSR1, catch_signal);
    signal(SIGUSR2, catch_signal);
#endif
#ifdef __UNIX__
    signal(SIGQUIT, catch_signal);
    signal(SIGHUP,  catch_signal);
    signal(SIGPIPE, catch_SIGPIPE);
    signal(SIGCHLD, catch_SIGCHLD);
#endif
}

#ifdef __UNIX__
void catch_SIGPIPE(int sig) {
    signal(SIGPIPE,  catch_SIGPIPE);
}

void catch_SIGCHLD(int sig) {
    waitpid(-1, NULL, WNOHANG);
    signal(SIGCHLD, catch_SIGCHLD);
}
#endif

void dump_core_and_exit(void) {
    uninit_sig();
    abort();
}

void catch_SIGFPE(int sig) {
    caught_fpe++;
    signal(SIGFPE,  catch_SIGFPE);
}

static const char *sig_name(int sig) {
    switch(sig) {
        case SIGILL:  return "ILL";
        case SIGSEGV: return "SEGV";
        case SIGINT:  return "INT";
#ifdef __UNIX__
        case SIGQUIT: return "QUIT";
        case SIGHUP:  return "HUP";
#endif
        case SIGTERM: return "TERM";
#if !defined(USING_GLIBC_2_0) && !defined(__MSVC__)
        case SIGUSR1: return "USR1";
        case SIGUSR2: return "USR2";
#endif
        default:      return "Unknown";
    }
    return NULL;
}

/* void catch_signal(int sig, int code, struct sigcontext *scp) { */
void catch_signal(int sig) {
    const char *sptr;
    cStr *sigstr;
    cData arg1;
    bool  do_shutdown = false;

    signal(sig, catch_signal);

    sptr = sig_name(sig);
    sigstr = string_from_chars(sptr, strlen(sptr));

    write_err("Caught signal %d: %S", sig, sigstr);

    string_discard(sigstr);


    /* figure out what to do */
    switch(sig) {
#ifdef __UNIX__
        case SIGHUP:
            atomic = false;
            handle_connection_output();
            flush_files();
#endif
#if !defined(USING_GLIBC_2_0) && !defined(__MSVC__)
        case SIGUSR2:
            /* let the db do what it wants from here */
            break;
        case SIGUSR1:
#else
        case SIGILL:
#endif
        {
            cData * d;
            cList * l;

            /* First cancel all preempted and suspended tasks */
            l = vm_list();
            for (d=list_first(l); d; d=list_next(l, d)) {
                /* boggle */
                if (d->type != INTEGER)
                    continue;
                vm_cancel(d->u.val);
            }
            list_discard(l);

            /* now cancel the current task if it is valid */
            if (vm_lookup(task_id) != NULL) {
                vm_cancel(task_id);
            }

            /* jump back to the main loop */
            longjmp(main_jmp, 1);
            break;
        }
#ifndef USING_GLIBC_2_0
        case SIGILL:
            /* lets panic and hopefully shutdown without frobbing the db */
            panic(sig_name(sig));
            break;
#endif
        case SIGTERM:
            if (running) {
                write_err("*** Attempting normal shutdown ***");
                running = false;

                /* jump back to the main loop, ignore any current tasks;
                   *drip*, *drip*, leaky */
                longjmp(main_jmp, 1);
            } else {
                panic(sig_name(sig));
            }
            break;
        default:
            do_shutdown = true;
            break;
    }

    /* only pass onto the db if we are 'executing' */
    if (!running)
        return;

    /* send a message to the system object */
    arg1.type = SYMBOL;
    arg1.u.symbol = ident_get(sptr);
    vm_task(SYSTEM_OBJNUM, signal_id, 1, &arg1);

    if (do_shutdown)
        running = false;
}

