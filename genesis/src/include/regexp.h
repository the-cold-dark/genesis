/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Definitions etc. for regexp(3) routines.
//
// Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
// not the System V one.
*/

#ifndef cdc_regexp_h
#define cdc_regexp_h

typedef struct regexp regexp;

#define NSUBEXP  10

struct regexp {
    char *startp[NSUBEXP];
    char *endp[NSUBEXP];
    char  regstart;          /* Internal use only. */
    char  reganch;           /* Internal use only. */
    char *regmust;           /* Internal use only. */
    int   regmlen;           /* Internal use only. */
    char  program[1];        /* Unwarranted chumminess with compiler. */
};

extern regexp * regcomp(char *exp);
extern int      regexec(regexp *prog, char *string, int case_flag);
extern int      regsub(regexp *prog, char *src, char *dest);
extern char   * regerror(char *msg);

#define	MAGIC	0234

#endif

