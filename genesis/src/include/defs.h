/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/defs.h
// ---
// Generic definitions, these usually dont change
*/

#ifndef _defs_h_
#define _defs_h_

/* fun with defines */
#ifndef CAT
  #ifdef __BORLANDC__
    #undef  _CAT
    #define _CAT(x)         x
    #define CAT(a,b)        _CAT(a)_CAT(b)
  #else
    #ifdef __WATCOMC__
      #define _CAT(a,b)     a ## b
      #define CAT(a,b)      _CAT(a,b)
    #else
      #define CAT(a,b)      a ## b
    #endif
  #endif
#endif

/* GRR linux; just get FreeBSD, it is faster */
#ifdef sys_linux
#undef NULL
#define NULL 0
#endif

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#ifndef _grammar_y_
#include "parse.h"
#endif
#include "cdc_types.h"

jmp_buf main_jmp;

char * c_dir_binary;
char * c_dir_textdump;
char * c_dir_bin;
char * c_dir_root;
char * c_logfile;
char * c_errfile;
char * c_pidfile;

FILE * logfile;
FILE * errfile;
string_t * str_tzname;

int  c_interactive;
int  running;
int  atomic;
int  heartbeat_freq;

void init_defs(void);

#define NEW_OVERRIDE

/* Number of ticks a method gets before dying with an E_TICKS. */
#define METHOD_TICKS		20000

/* Number of ticks a paused method gets before dying with an E_TICKS */
#define PAUSED_METHOD_TICKS     5000

/* How much of a threshold refresh() should decide to pause on */
#define REFRESH_METHOD_THRESHOLD   500

/* Maximum depth of method calls. */
#define MAX_CALL_DEPTH		128

/* Clean objects out of the cache?  This gives you a smaller memory
   imprint, but it may cause lag if your memory cache is too small
   and you are demanding many objects constantly.  You may want to
   expand the cache size */
#define CLEAN_CACHE

/* How persistant is an object, to stay in the cache?  This is
   logarithmic, just changing it from 10 to 20 will not double the
   persistance, it will just increase it a notch */
#define OBJECT_PERSISTANCE 10

/* Width and depth of object cache. (10 and 30 are defaults) */
#define CACHE_WIDTH	10
#define CACHE_DEPTH	30

#define FORCED_CLEANUP_LIMIT 64
#define FORCED_CLEANUP_BOUND 40

/* size of name cache, this number is total magic--although primes make
   for better magic, lower for less memory usage but more lookups to
   disk; raise for vise-versa, other primes:

       71, 173, 281, 409, 541, 659, 809

*/
#define NAME_CACHE_SIZE 173

/* Default indent for decompiled code. */
#define DEFAULT_INDENT	4

/* Maximum number of characters of a data value to display using format(). */
#define MAX_DATA_DISPLAY 15

#define INV_OBJNUM	-1
#define SYSTEM_OBJNUM	0
#define ROOT_OBJNUM	1

/* sizes */

#define WORD   32
#define LINE   80
#define BUF    256
#define BIGBUF 1024
#define IOBUF  8192

#define F_SUCCESS 0  /* function absolute success */
#define F_FAILURE -1 /* function failure */
#define B_SUCCESS 1  /* boolean success */
#define B_FAILURE 0  /* boolean failure */
#define TRUE 1
#define FALSE 0
#define ANY_TYPE 0

#define DISABLED 0   /* I use this in place of #if 0, -Brandon */

#define INTERNAL static

#define SERVER_NAME "Genesis (the ColdX driver)"

#define DEF_BLOCKSIZE 512

/* eventually allow double */
#define FLOAT_TYPE float

#endif
