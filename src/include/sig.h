/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_signal_h
#define cdc_signal_h

#include <stdnoreturn.h>

void init_sig(void);

extern short caught_fpe;   /* if we catch SIGFPE */

/* void catch_signal(int sig, int code, struct sigcontext *scp); */
void catch_signal(int sig);
noreturn void dump_core_and_exit(void);

#endif

