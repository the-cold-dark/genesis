/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/log.h
// ---
// Declarations for logging and panic routines.
*/

#ifndef LOG_H
#define LOG_H

void panic(char *s, ...);
void fail_to_start(char *s);
void write_log(char *s, ...);
void write_err(char *s, ...);

#endif

