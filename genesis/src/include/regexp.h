/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/regexp.h
// ---
// Definitions etc. for regexp(3) routines.
//
// Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
// not the System V one.
*/

#ifndef REGEXP_H
#define REGEXP_H

typedef struct regexp regexp;

#define NSUBEXP  10
struct regexp {
	char *startp[NSUBEXP];
	char *endp[NSUBEXP];
	char regstart;		/* Internal use only. */
	char reganch;		/* Internal use only. */
	char *regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	char program[1];	/* Unwarranted chumminess with compiler. */
};

extern regexp *regcomp(char *exp);
extern int regexec(regexp *prog, char *string, int case_flag);
extern int regsub(regexp *prog, char *src, char *dest);
extern char *regerror(char *msg);

#endif

