/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/sig.h
// ---
// Declarations for Coldmud signal-handling routines.
*/

#ifndef SIGNAL_H
#define SIGNAL_H

void init_sig(void);

/* void catch_signal(int sig, int code, struct sigcontext *scp); */
void catch_signal(int sig);

#endif

