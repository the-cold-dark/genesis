/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "execute.h"      /* task() */
#include "sig.h"

void catch_SIGCHLD(int sig);
void catch_SIGFPE(int sig);

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
    caught_fpe = 0;
    signal(SIGFPE,  catch_SIGFPE);
    signal(SIGILL,  catch_signal);
    signal(SIGQUIT, catch_signal);
    signal(SIGINT,  catch_signal);
    signal(SIGHUP,  catch_signal);
    signal(SIGTERM, catch_signal);
    signal(SIGUSR1, catch_signal);
    signal(SIGUSR2, catch_signal);
    signal(SIGCHLD, catch_SIGCHLD);
}

void catch_SIGFPE(int sig) {
    caught_fpe++;
}

void catch_SIGCHLD(int sig) {
    waitpid(-1, NULL, WNOHANG);

    /* reset the signal so we catch it again */
    signal(SIGCHLD, catch_SIGCHLD);
}

char *sig_name(int sig) {
    switch(sig) {
        case SIGILL:  return "ILL";
        case SIGQUIT: return "QUIT";
        case SIGSEGV: return "SEGV";
        case SIGINT:  return "INT";
        case SIGHUP:  return "HUP";
        case SIGTERM: return "TERM";
        case SIGUSR1: return "USR1";
        case SIGUSR2: return "USR2";
        default:      return "Unknown";
    }
    return NULL;
}

/* void catch_signal(int sig, int code, struct sigcontext *scp) { */
void catch_signal(int sig) {
    char *sptr;
    cStr *sigstr;
    cData arg1;
    Bool  do_shutdown = NO;

    signal(sig, catch_signal);

    sptr = sig_name(sig);
    sigstr = string_from_chars(sptr, strlen(sptr));

    write_err("Caught signal %d: %S", sig, sigstr);

    /* figure out what to do */
    switch(sig) {
        case SIGHUP:
            atomic = NO;
            handle_connection_output();
            flush_files();
        case SIGUSR2:
            /* let the db do what it wants from here */
            break;
        case SIGUSR1: {
            cData * d;
            cList * l;
 
            /* First cancel all preempted and suspended tasks */
            l = task_list();
            for (d=list_first(l); d; d=list_next(l, d)) {
                /* boggle */
                if (d->type != INTEGER)
                    continue;
                task_cancel(d->u.val);
            }
            list_discard(l);

            /* now cancel the current task */
            task_cancel(task_id);

            /* jump back to the main loop */
            longjmp(main_jmp, 1);
            break;
        }
        case SIGILL:
            /* lets panic and hopefully shutdown without frobbing the db */
            panic(sig_name(sig));
            break;
        case SIGTERM:
            if (running) {
                write_err("*** Attempting normal shutdown ***");
                running = NO;

                /* jump back to the main loop, ignore any current tasks;
                   *drip*, *drip*, leaky */
                longjmp(main_jmp, 1);
            } else {
                panic(sig_name(sig));
            }
            break;
        default:
            do_shutdown = YES;
            break;
    }

    /* only pass onto the db if we are 'executing' */
    if (!running)
        return;

    /* send a message to the system object */
    arg1.type = SYMBOL;
    arg1.u.symbol = ident_get(string_chars(sigstr));
    task(SYSTEM_OBJNUM, signal_id, 1, &arg1);

    if (do_shutdown)
        running = NO;
}

