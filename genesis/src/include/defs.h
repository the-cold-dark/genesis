/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_defs_h
#define cdc_defs_h

#define DISABLED 0
#define ENABLED  1

/*
// ---------------------------------------------------------------------
// This will reduce how much your database bloats, however it will
// be slower as it searches the whole database for free blocks.
// For now this is an option to reduce the amount of bloat occuring,
// until an alternate allocator is created.  If you are having a problem
// with your database quickly bloating in size, it is suggested to
// enable this option, unless you have a slow disk or slow disk device.
*/
#if DISABLED
#  define LOWBLOAT_DB
#endif

/*
// ---------------------------------------------------------------------
// Use larger storage for floats and integers.  This gives greater
// precision, but is not necessarily recommended unless you know your
// system can handle 64bit + words.  You do not need to specify both
// (You can specify just USE_BIG_FLOATS, and not numbers).
*/
#if DISABLED
#  define USE_BIG_FLOATS
#endif

#if DISABLED
#  define USE_BIG_NUMBERS
#endif

/*
// ---------------------------------------------------------------------
// This enables execution debuging in the ColdC language--using the
// functions debug_callers() and call_trace().
*/

#if ENABLED
#  define DRIVER_DEBUG
#endif


/*
// ---------------------------------------------------------------------
// Turn this on to get a profile of what methods are called and how often,
// do NOT use this in a regular run-time environment as its laggy
*/
#if DISABLED
#  define PROFILE_EXECUTE
#endif

/*
// ---------------------------------------------------------------------
// This is the number of methods it can record before having to flush
// the table and start over, if you are getting a lot tack a zero or two
// onto this number.  Keep in mind it will take up MEMORY
*/
#define PROFILE_MAX 10000

/*
// ---------------------------------------------------------------------
// This is what the core execution loop should wait (seconds), if no
// heartbeat is set (this will not add a heartbeat)
*/
#define NO_HEARTBEAT_INTERVAL 2

/*
// ---------------------------------------------------------------------
// Clean objects out of the cache?  This gives you a smaller memory
// imprint, but it usually causes more work for the server as it will
// be constantly removing and pulling objects in an dout of the cache.
*/
#if DISABLED
#  define CLEAN_CACHE
#endif

/*
// ---------------------------------------------------------------------
// How persistant is an object, to stay in the cache?  This is
// logarithmic, just changing it from 10 to 20 will not double the
// persistance, it will just increase it a notch
*/
#define OBJECT_PERSISTANCE 10

/*
// ---------------------------------------------------------------------
// Number of ticks a method gets before dying with an E_TICKS.
*/
#define METHOD_TICKS               20000

/*
// ---------------------------------------------------------------------
// Number of ticks a paused method gets before dying with an E_TICKS
*/
#define PAUSED_METHOD_TICKS        5000

/*
// ---------------------------------------------------------------------
// How much of a threshold refresh() should decide to pause on
*/
#define REFRESH_METHOD_THRESHOLD   500

/*
// ---------------------------------------------------------------------
// Maximum depth of method calls.
*/
#define MAX_CALL_DEPTH             128

/*
// ---------------------------------------------------------------------
// size of name cache, this number is total magic--although primes make
// for better magic, lower for less memory usage but more lookups to
// disk; raise for vise-versa, other primes:
//
//     71, 173, 281, 409, 541, 659, 809
*/
#define NAME_CACHE_SIZE 173

/*
// ---------------------------------------------------------------------
// Default indent for decompiled code.
*/
#define DEFAULT_INDENT    4

/*
// ---------------------------------------------------------------------
// Maximum number of characters of a data value to display using strfmt().
*/
#define MAX_DATA_DISPLAY 15

/*
// --------------------------------------------------------------------
// global behaviour defines
// --------------------------------------------------------------------
*/

/* core behaviour defines, set by configure */
#include "config.h"

#ifndef CAT
#  ifdef __WATCOMC__
#    define _CAT(a,b)     a ## b
#    define CAT(a,b)      _CAT(a,b)
#  else
#    define CAT(a,b)      a ## b
#  endif
#endif

#define INV_OBJNUM        -1
#define SYSTEM_OBJNUM     0
#define ROOT_OBJNUM       1

#ifdef USE_VFORK
#define FORK_PROCESS vfork
#else
#define FORK_PROCESS fork
#endif

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
#define strerror(n) (sys_errlist[n])
#endif

/*
// bool defs pulled from the Perl5 Source, Copyright 1991-1994, Larry Wall
*/
#ifdef _G_HAVE_BOOL
#  if _G_HAVE_BOOL
#    ifndef HAS_BOOL
#      define HAS_BOOL 1
#    endif
#  endif
#endif

#ifndef HAS_BOOL
#  ifdef UTS      /* what is UTS, need documentation -- BJG */
#    define bool int
#  else
#    define bool char 
#  endif
#endif

/*
// these are C type defines, the following should be true:
//
//   Bool     =>  true or false value
//   Byte     =>  tiny integer signed value
//   uByte    =>  tiny integer unsigned value
//   Char     =>  character signed value
//   uChar    =>  character unsigned value
//   Short    =>  small integer signed value
//   uShort   =>  small integer unsigned value
//   Int      =>  common integer signed value
//   uInt     =>  common integer unsigned value
//   Long     =>  large integer signed value
//   uLong    =>  large integer unsigned value
//   Float    =>  common float signed value
//
// Defining USE_LARGE_FLOATS and USE_LARGE_NUMBERS will change Long and
// Float to be larger, although this is not recommended unless you know
// your OS and system can handle larger 64 bit + words.
//
// As machines advance we may need to change these #if's to handle larger
// than 64 bits, for now they assume 64bits is the heavenly bit in the sky.
*/

typedef bool              Bool;

#if SIZEOF_CHAR == 1
  typedef char               Byte;
  typedef unsigned char      uByte;
  typedef char               Char;
  typedef unsigned char      uChar;
#elif SIZEOF_SHORT == 1
  typedef short int          Byte;
  typedef unsigned short int uByte;
  typedef char               Char;
  typedef unsigned char      uChar;
#else
# error "Unable to specify size for Byte and Char type (8 bits)"
#endif

#if SIZEOF_SHORT == 2
  typedef short int          Short;
  typedef unsigned short int uShort;
#elif SIZEOF_CHAR == 2   /* unlikely */
  typedef char               Short;
  typedef unsigned char      uShort;
#elif SIZEOF_INT == 2
  typedef int                Short;
  typedef unsigned int       uShort;
#else
# error "Unable to specify size for Short type (16 bits)"
#endif

 /* when monkeys fly */
#if SIZEOF_SHORT == 4
  typedef short int          Int;
  typedef unsigned short int uInt;
#elif SIZEOF_INT == 4
  typedef int               Int;
  typedef unsigned int      uInt;
# define MAX_INT  2147483647
# define MIN_INT  (- MAX_INT -1)
# define MAX_UINT 4294967295U
#elif SIZEOF_LONG == 4
  typedef long              Int;
  typedef unsigned long     uInt; 
# define MAX_INT  2147483647L
# define MIN_INT  (- MAX_INT - 1)
# define MAX_UINT 4294967295UL
#else
# error "Unable to specify size for Int type (32 bits)"
#endif

#ifdef USE_BIG_NUMBERS

#  if SIZEOF_LONG == 8
     typedef long              Long;
     typedef unsigned long     uLong;
#    define MAX_LONG  9223372036854775807L
#    define MIN_LONG  (- MAX_LONG - 1)
#    define MAX_ULONG 18446744073709551615UL
#  elif SIZEOF_LLONG == 8
     typedef long long         Long;
     typedef unsigned long long uLong;
#    define MAX_LONG  9223372036854775807LL
#    define MIN_LONG  (- MAX_LONG - 1)
#    define MAX_ULONG 18446744073709551615ULL
#  else
#    error "Unable to specify size for BIG Long type (64 bits)"
#  endif
#else

  typedef Int               Long;
  typedef uInt              uLong;

# define MAX_LONG  MAX_INT
# define MIN_LONG  (- MAX_INT - 1);
# define MAX_ULONG MAX_UINT
#endif

#ifdef USE_BIG_FLOATS
#  if SIZEOF_FLOAT == 8     /* hah, not likely */
     typedef float           Float;
#  elif SIZEOF_DOUBLE == 8
     typedef double          Float;
#  elif SIZEOF_LDOUBLE == 8
     typedef double          Float;
#  else
#    error "Unable to specify size for BIG Float type (64 bits)"
#  endif
#else
#  if SIZEOF_FLOAT == 4
     typedef float           Float;
#  elif SIZEOF_DOUBLE == 4   /* will this ever occur !? */
     typedef double          Float;
#  else
#    error "Unable to specify size for Float type (32 bits)"
#  endif
#endif

/* basic sizes */
#define LINE   80
#define BUF    256
#define BLOCK  512
#define BIGBUF 1024
#define IOBUF  8192

/* used by some file operations */
#define DEF_BLOCKSIZE BLOCK

#ifndef __Win32__
#  define SOCKET_ERROR    -1
#endif

#define F_SUCCESS 0  /* function absolute success */
#define F_FAILURE -1 /* function failure */
#define B_SUCCESS 1  /* boolean success */
#define B_FAILURE 0  /* boolean failure */
#ifdef TRUE
#undef TRUE
#endif
#define TRUE 1

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#ifdef NO
#undef NO
#endif
#define NO FALSE

#ifdef YES
#undef YES
#endif
#define YES TRUE

#define ANY_TYPE 0

/* personal preference stuff */
#define forever for (;;)
#define INTERNAL static

#define SERVER_NAME "Genesis (the Cold driver)"

#ifdef __MSVC__
#define S_ISDIR(mode) (mode & _S_IFDIR)
#endif

/* incase it doesn't exist */
#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
// --------------------------------------------------------------------
// standard includes
// --------------------------------------------------------------------
*/

#if 0
#if defined(__UNIX__) && !defined(USG)
#include <sys/param.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef sys_linux
#undef NULL
#define NULL 0
#endif

#include <setjmp.h>
#include "cdc_errs.h"
#include "cdc_types.h"
#include "cdc_memory.h"
#include "log.h"

#ifndef _grammar_y_
#include "parse.h"

extern cInstance class_registry[];

typedef enum instance_id {
    FIRST_INSTANCE = LAST_TOKEN + 1,
    HANDLED_FROB_TYPE = FIRST_INSTANCE,
    LAST_INSTANCE
} InstanceID;

void register_instance (InstanceID instance, Ident id);

#endif


/*
// --------------------------------------------------------------------
// globals, aiee
// --------------------------------------------------------------------
*/

#ifdef DEFS_C
jmp_buf main_jmp;

char * c_dir_binary;
char * c_dir_textdump;
char * c_dir_bin;
char * c_dir_root;
char * c_logfile;
char * c_errfile;
char * c_runfile;

FILE * logfile;
FILE * errfile;
cStr * str_tzname;
cStr * str_hostname;
cStr * str_release;
cStr * str_system;

Bool coldcc;
Bool running;
Bool atomic;
Int  heartbeat_freq;

Int cache_width;
Int cache_depth;

void init_defs(void);

/* limits configurable with 'config()' */
Int  limit_datasize;
Int  limit_fork;
Int  limit_calldepth;
Int  limit_recursion;
Int  limit_objswap;

#else
extern jmp_buf main_jmp;

extern char * c_dir_binary;
extern char * c_dir_textdump;
extern char * c_dir_bin;
extern char * c_dir_root;
extern char * c_logfile;
extern char * c_errfile;
extern char * c_runfile;

extern FILE * logfile;
extern FILE * errfile;
extern cStr * str_tzname;
extern cStr * str_hostname;
extern cStr * str_release;
extern cStr * str_system;

extern Bool coldcc;
extern Bool running;
extern Bool atomic;
extern Int  heartbeat_freq;

extern Int cache_width;
extern Int cache_depth;

extern void init_defs(void); 

/* limits configurable with 'config()' */
extern Int  limit_datasize;
extern Int  limit_fork;
extern Int  limit_calldepth;
extern Int  limit_recursion;
extern Int  limit_objswap;

#endif

#endif

