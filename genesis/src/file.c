/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: file.c
// ---
// File routines.
//
// Some of these routines are used in io.c, primarily when updating i/o
// io.c modifies and uses 'files'.
*/

#define _file_

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "file.h"
#include "execute.h"
#include "memory.h"
#include "grammar.h"
#include "cdc_types.h"
#include "data.h"
#include "util.h"
#include "cache.h"
#include "log.h"

/*
// --------------------------------------------------------------------
// The first routines deal with file controllers, and should be system
// inspecific.
// --------------------------------------------------------------------
*/

/*
// --------------------------------------------------------------------
// this is called _AFTER_ the file has been flushed and closed.
*/
void file_discard(filec_t * file) {
    /* clear the object's file variable */
    object_t * obj = cache_retrieve(file->dbref);
    if (obj != NULL) {
        obj->file = NULL;
        cache_discard(obj);
    }

    if (file->wbuf)
        buffer_discard(file->wbuf);
    if (file->rbuf)
        buffer_discard(file->rbuf);

    efree(file);
}

filec_t * file_new(int blocksize) {
    filec_t * fnew = EMALLOC(filec_t, 1);

    fnew->fp = NULL;
    fnew->rbuf = NULL;
    fnew->wbuf = NULL;
    fnew->dbref = INV_OBJNUM;
    fnew->block = blocksize;
    fnew->flags.readable = 0;
    fnew->flags.writable = 0;
    fnew->flags.closed = 0;
    fnew->next = NULL;

    return fnew;
}

int close_file(filec_t * file) {
    flush_file(file);
    fclose(file->fp);
    file->flags.closed = 1;

    return 1;
}

int flush_file(filec_t * file) {
    if (file->flags.writable) {
        while (file->wbuf)
            file_write(file);
    }

    return 1;
}

int read_file(filec_t * file) {
    size_t   size;
    Buffer * buf;

    if (!feof(file->fp)) {
        buf = buffer_new(file->block);
        size = fread(buf->s, sizeof(unsigned char), file->block, file->fp);
        buf->len = size;
        buffer_append(file->rbuf, buf);
        buffer_discard(buf);

        return 1;
    }

    return 0;
}

/* assumes the file is writable, to reduce checks */
void file_write(filec_t * file) {
    if (file->wbuf->len) {
        if (file->wbuf->len > file->block) {
            fwrite(file->wbuf->s, sizeof(unsigned char), file->block, file->fp);
            file->wbuf = buffer_tail(file->wbuf, file->block+1);
        } else {
            fwrite(file->wbuf->s, sizeof(unsigned char), file->wbuf->len, file->fp);
            buffer_discard(file->wbuf);
            file->wbuf = NULL;
        }
    }
}

internal filec_t * find_file_controller(object_t * obj) {

    /* obj->file is only for faster lookups */
    if (obj->file == NULL) {
        filec_t * file;

        /* lets try and find the file */
        for (file = files; file; file = file->next) {
            if (file->dbref == obj->dbref && !file->flags.closed) {
                obj->file = file;
                break;
            }
        }
    }

    /* it could still be NULL */
    return obj->file;
}

int abort_file(object_t * obj) {
    filec_t * file = find_file_controller(obj);

    if (file != NULL) {
        close_file(file);
        return 1;
    }

    return 0;
}

int stat_file(object_t * obj, struct stat * sbuf) {
    filec_t * file = find_file_controller(obj);

    if (file != NULL) {
#if 0
        fstat(, sbuf);
#endif
        return 1;
    }

    return 0;
}

/*
// --------------------------------------------------------------------
// These routines are file utilities, and will probably not work too
// well on anything but unix.
// --------------------------------------------------------------------
*/

int build_path(char * buf, char * fname, int size) {
    int         len = strlen(fname);

    if (len == 0)
        return FE_INVPATH;

    if (len + strlen(c_dir_root) + 1 > size)
        return FE_LONGNAME;

#ifdef RESTRICTIVE_FILES

    if (strstr(fname, "../"))
        return FE_INVPATH;

    strncpy(buf, c_dir_root, size);
    strncat(buf, "/", size);
    strncat(buf, fname, size);

#else

    strncpy(buf, fname, size);

#endif

    return 0;
}

int find_file(char * fname, struct stat * statbuf) {
    if (stat(fname, statbuf) < 0)
        return FE_NOFILE;

    if (S_ISDIR(statbuf->st_mode))
        return FE_ISDIR;

    return 0;
}

#define ABORT(__s) { file_discard(fnew); return __s; }

int open_file(char * fname, char * perms, filec_t ** file, int blocksize) {
    char        buf[BUF];
    filec_t    * fnew = file_new(blocksize);
    int         status;

    if (!strlen(perms))
        ABORT(FE_INVPERMS);

    switch (perms[0]) {
        case 'r':
            fnew->flags.readable = 1;
            break;
        case 'w':
        case 'a':
            fnew->flags.writable = 1;
            break;
    }

    status = build_path(buf, fname, BUF);
    if (status != 0)
        ABORT(status);

    status = find_file(buf, &fnew->statbuf);
    if (status != F_SUCCESS)
        ABORT(status);

    fnew->fp = fopen(buf, perms);

    if (fnew->fp == NULL) {
        switch (errno) {
            case EACCES:  status = FE_ACCESS; break;
            case EPERM:   status = FE_PERM;   break;
            case EINTR:   status = FE_INTR;   break;
            case ELOOP:   status = FE_LOOP;   break;
            case EMFILE:  status = FE_MFILE;  break;
            case ENFILE:  status = FE_NFILE;  break;
            case ENOENT:  status = FE_NOENT;  break;
            case ENOTDIR: status = FE_NOTDIR; break;
            case EROFS:   status = FE_ROFS;   break;
            case ETXTBSY: status = FE_TXTBSY; break;
            case ENOMEM:
                panic("Unable to allocate memory for file buffer!");
                break;
            case ENOSPC:
                panic("Unable to open file: not enough disk space!");
                break;
            default: status = FE_UNKNOWN;
        }
        ABORT(status);
    }

    *file = fnew;

    return 1;
}

/*
// --------------------------------------------------------------------
*/
/*

 fstat() (was stat_file(), get stat info on a file:
          bytes, blocks, type, mode, owner, group, last access times)
+fchmod()
+fmkdir()
+frmdir()
+files()   (returns files in a directory, a list)
+fremove()
+frename()

+fopen()
+fclose()
+fseek()

 execute()
+file() (if a file is bound to this object, returns basic info)

*/

/*
// --------------------------------------------------------------------
// called by fread()
*/
Buffer * read_from_file(object_t * obj) {
    Buffer * buf;
    filec_t * file = find_file_controller(obj);

    if (file == NULL)
        return NULL;

    if (!file->rbuf) {
        if (feof(file->fp))
            return NULL;
        read_file(file);
    }

    buf = file->rbuf;
    file->rbuf = NULL;

    return buf;
}

/*
// --------------------------------------------------------------------
// called by fwrite()
*/
int write_to_file(object_t * obj, Buffer * buf) {
    filec_t * file = find_file_controller(obj);

    if (file == NULL || !file->flags.writable)
        return 0;

    if (file->wbuf)
        file->wbuf = buffer_append(file->wbuf, buf);
    else
        file->wbuf = buffer_dup(buf);

    return 1;
}

/*
// --------------------------------------------------------------------
// called by feof()
*/
int file_end(object_t * obj) {
    filec_t * file = find_file_controller(obj);

    if (file == NULL)
        return F_FAILURE;

    return (feof(file->fp));
}

#undef _file_
