/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/file.h
// ---
// Declarations for file management.
*/

#ifndef _file_h_
#define _file_h_

/* file controller, was file_t but it conflicted on some OS's */
typedef struct file_s       filec_t;

#include <sys/types.h>
#include <sys/stat.h>
#include "cdc_types.h"

struct file_s {
    FILE   * fp;
    Buffer * rbuf;
    Buffer * wbuf;
    Dbref    dbref;
    int      block;
    struct {
        unsigned int readable : 1;
        unsigned int writable : 1;
        unsigned int closed   : 1;
    } flags;
    struct stat statbuf;

    filec_t * next;
};

void file_discard(filec_t * file);
filec_t * file_new(int blocksize);
int close_file(filec_t * file);
int flush_file(filec_t * file);
int read_file(filec_t * file);
void file_write(filec_t * file);
int abort_file(object_t * obj);
int stat_file(object_t * obj, struct stat * sbuf);

#ifdef _file_

filec_t * files;            /* List of files the db is using */

#else

extern filec_t * files;     /* List of files the db is using */

#endif

#define FE_INVPERMS 1
#define FE_ISDIR    2
#define FE_NOFILE   3
#define FE_INVPATH  4
#define FE_INVNAME  5
#define FE_LONGNAME 6
#define FE_ACCESS   7     /* EACCES */
#define FE_PERM     8     /* EPERM */
#define FE_INTR     9     /* EINTR */
#define FE_LOOP     10    /* ELOOP */
#define FE_MFILE    11    /* EMFILE */
#define FE_NFILE    12    /* ENFILE */
#define FE_NOENT    13    /* ENOENT */
#define FE_NOTDIR   14    /* ENOTDIR */
#define FE_ROFS     15    /* EROFS */
#define FE_TXTBSY   16    /* ETXTBSY */
#define FE_UNKNOWN  100

#endif

