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

#include "config.h"
#include "defs.h"

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
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

filec_t * files = NULL;

void flush_files(void) {
    filec_t * file;

    for (file = files; file; file = file->next)
        flush_file(file);
}

void close_files(void) {
    filec_t * file,
            * old;

    file = files;
    while (file) {
        close_file(file);
        old = file;
        file = file->next;
        file_discard(old, NULL);
    }
}

/*
// --------------------------------------------------------------------
//
// NOTE: If you send the object along, it is assumed it is the CORRECT
// object bound to this function, sending the wrong object can cause
// problems.
//
*/
void file_discard(filec_t * file, object_t * obj) {
    filec_t  ** fp, * f;

    /* clear the object's file variable */
    if (obj == NULL) {
        if (file->objnum != INV_OBJNUM) {
            object_t * obj = cache_retrieve(file->objnum);

            if (obj != NULL) {
                obj->file = NULL;
                cache_discard(obj);
            }
        }
    } else {
        obj->file = NULL;
    }

    /* pull it out of the 'files' list */
    fp = &files;
    while (*fp) {
        f = *fp;
        if (f->objnum == file->objnum) {
            if (!f->f.closed)
                close_file(f);
            *fp = f->next;
            break;
        } else {
            fp = &f->next;
        }
    }

    /* toss the file proper */
    string_discard(file->path);

    efree(file);
}

filec_t * file_new(void) {
    filec_t * fnew = EMALLOC(filec_t, 1);

    fnew->fp = NULL;
    fnew->objnum = INV_OBJNUM;
    fnew->f.readable = 0;
    fnew->f.writable = 0;
    fnew->f.closed = 0;
    fnew->f.binary = 0;
    fnew->path = NULL;
    fnew->next = NULL;

    return fnew;
}

void file_add(filec_t * file) {
    file->next = files;
    files = file;
}

filec_t * find_file_controller(object_t * obj) {

    /* obj->file is only for faster lookups,
       and will go away when written to disk. */
    if (obj->file == NULL) {
        filec_t * file;

        /* lets try and find the file */
        for (file = files; file; file = file->next) {
            if (file->objnum == obj->objnum && !file->f.closed) {
                obj->file = file;
                break;
            }
        }
    }

    /* it may still be NULL */
    return obj->file;
}

int close_file(filec_t * file) {
    file->f.closed = 1;
    if (fclose(file->fp) == EOF)
        return errno;
    return 0;
}

int flush_file(filec_t * file) {
    if (file->f.writable) {
        if (fflush(file->fp) == EOF)
            return errno;
        return F_SUCCESS;
    }

    return F_FAILURE;
}

Buffer * read_binary_file(filec_t * file, int block) {
    Buffer * buf = buffer_new(block);

    if (feof(file->fp)) {
        cthrow(eof_id, "End of file.");
        return NULL;
    }

    buf->len = fread(buf->s, sizeof(unsigned char), block, file->fp);

    return buf;
}

string_t * read_file(filec_t * file) {
    string_t * str;

    if (feof(file->fp)) {
        cthrow(eof_id, "End of file.");
        return NULL;
    }

    str = fgetstring(file->fp);
    if (str == NULL)
        cthrow(eof_id, "End of file.");

    return str;
}

int abort_file(filec_t * file) {
    if (file != NULL) {
        close_file(file);
        file_discard(file, NULL);
        return F_SUCCESS;
    }

    return F_FAILURE;
}

int stat_file(filec_t * file, struct stat * sbuf) {
    if (file != NULL) {
        stat(file->path->s, sbuf);
        return F_SUCCESS;
    }

    return F_FAILURE;
}

/*
// --------------------------------------------------------------------
// These routines are file utilities, and will probably not work too
// well on anything but unix.
// --------------------------------------------------------------------
*/

string_t * build_path(char * fname, struct stat * sbuf, int nodir) {
    int         len = strlen(fname);
    string_t  * str = NULL;

    if (len == 0) {
        cthrow(file_id, "No file specified.");
        return NULL;
    }

#ifdef RESTRICTIVE_FILES
    if (strstr(fname, "../") || strstr(fname, "/..") || !strcmp(fname, "..")) {
        cthrow(perm_id, "Filename \"%s\" is not legal.", fname);
        return NULL;
    }

    str = string_from_chars(c_dir_root, strlen(c_dir_root));
    str = string_addc(str, '/');
    str = string_add_chars(str, fname, len);
#else
    if (*fname != '/') {
        str = string_from_chars(c_dir_root, strlen(c_dir_root));
        str = string_addc(str, '/');
        str = string_add_chars(str, fname, len);
    } else {
        str = string_from_chars(fname, len);
    }
#endif

    if (sbuf != NULL) {
        if (stat(str->s, sbuf) < 0) {
            cthrow(file_id, "Cannot find file \"%s\".", str->s);
            string_discard(str);
            return NULL;
        }
        if (nodir) {
            if (S_ISDIR(sbuf->st_mode)) {
                cthrow(directory_id, "\"%s\" is a directory.", str->s);
                string_discard(str);
                return NULL;
            }
        }
    }

    return str;
}

list_t * statbuf_to_list(struct stat * sbuf) {
    list_t       * list;
    data_t       * d;
    register int   x;

    list = list_new(5);
    d = list_empty_spaces(list, 5);
    for (x=0; x < 5; x++)
        d[x].type = INTEGER;

    d[0].u.val = (int) sbuf->st_mode;
    d[1].u.val = (int) sbuf->st_size;
    d[2].u.val = (int) sbuf->st_atime;
    d[3].u.val = (int) sbuf->st_mtime;
    d[4].u.val = (int) sbuf->st_ctime;

    return list;
}

list_t * open_file(string_t * name, string_t * smode, object_t * obj) {
    char        mode[4];
    int         rw = 0;
    char      * s = NULL;
    filec_t   * fnew = file_new();
    struct stat sbuf;

    /* parse the mode first, if the string pointer is NULL, set it readable */
    if (smode != NULL) {
        s = smode->s;
        if (*s == '+') {
            rw = 1;
            fnew->f.readable = fnew->f.writable = 1;
            s++;
        }
    
        if (*s == '>') {
            s++;
            if (*s == '>') {
                s++;
                mode[0] = 'a';
            } else {
                mode[0] = 'w';
            }
            fnew->f.writable = 1;
        } else {
            if (*s == '<' )
                s++;
            mode[0] = 'r';
            fnew->f.readable = 1;
        }

        /* here is where we branch from perl, '-' is used to specify it as
           a 'binary' file (i.e. use buffers not cold strings) */
        if (*s == '-') {
            s++;
            fnew->f.binary = 1;
        }

    } else {
        mode[0] = 'r';
        fnew->f.readable = 1;
    }

    /* most systems ignore this, some need it */
    mode[1] = 'b';

    if (rw) {
        mode[2] = '+';
        mode[3] = (char) NULL;
    } else {
        mode[2] = (char) NULL;
    }

    fnew->path = build_path(name->s, NULL, DISALLOW_DIR);
    if (fnew->path == NULL)
        return NULL;

    /* redundant, as build_path could have done this, but we
       have a special case which we need to handle differently */

    if (stat(fnew->path->s, &sbuf) == F_SUCCESS) {
        if (S_ISDIR(sbuf.st_mode)) {
            cthrow(directory_id, "\"%s\" is a directory.", fnew->path->s);
            return NULL;
        }
    }

    fnew->fp = fopen(fnew->path->s, mode);

    if (fnew->fp == NULL) {
        if (errno == ENOMEM)
            panic("open_file(): %s", strerror(errno));
        cthrow(file_id, "%s (%s)", strerror(errno), name->s);
        file_discard(fnew, NULL);
        return NULL;
    }

    file_add(fnew);
    obj->file = fnew;
    fnew->objnum = obj->objnum;

    return statbuf_to_list(&sbuf);
}

#if DISABLED
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
        file_read(file);
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

    if (file == NULL || !file->f.writable)
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
#endif

#undef _file_
