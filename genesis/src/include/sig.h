/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_signal_h
#define cdc_signal_h

void init_sig(void);

#ifdef SIG_C
short caught_fpe;   /* if we catch SIGFPE */
#else
extern short caught_fpe;
#endif

/* void catch_signal(int sig, int code, struct sigcontext *scp); */
void catch_signal(int sig);
void dump_core_and_exit(void);

#endif

