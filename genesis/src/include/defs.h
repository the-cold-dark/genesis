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

#ifndef _defs_

extern char c_dir_binary[32];
extern char c_dir_textdump[32];
extern char c_dir_bin[32];
extern char c_dir_root[32];
extern int  c_interactive;
extern int  running;
extern int  heartbeat_freq;

#endif

void init_defs(void);

#define NEW_OVERRIDE

/* Number of ticks a method gets before dying with an E_TICKS. */
#define METHOD_TICKS		20000

/* Number of ticks a paused method gets before dying with an E_TICKS */
#define PAUSED_METHOD_TICKS     5000

/* Maximum depth of method calls. */
#define MAX_CALL_DEPTH		128

/* Width and depth of object cache. (15 and 30 are defaults) */
#define CACHE_WIDTH	2
#define CACHE_DEPTH	50
#define FORCED_CLEANUP_LIMIT 64
#define FORCED_CLEANUP_BOUND 40

/* Default indent for decompiled code. */
#define DEFAULT_INDENT	4

/* Maximum number of characters of a data value to display using format(). */
#define MAX_DATA_DISPLAY 15

#define INV_OBJNUM	-1
#define SYSTEM_DBREF	0
#define ROOT_DBREF	1

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

#define DISABLED 0   /* I use this in place of #if 0, -Brandon */

#define internal static /* because I dislike static in function defs */

#define SERVER_NAME "Genesis (the ColdX driver)"
