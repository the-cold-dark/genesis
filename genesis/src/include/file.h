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

typedef struct filec_s       filec_t;

#include <sys/types.h>
#include <sys/stat.h>
#include "cdc_types.h"

#define DISALLOW_DIR 1
#define ALLOW_DIR 0

struct filec_s {
    FILE     * fp;
    objnum_t   objnum;
    filec_t  * next;
    string_t * path;
#if 0
    int        (*read)(filec_t * file, data_t * d);
    int        (*write)(filec_t * file, data_t * d);
#endif
    struct {
        unsigned int readable : 1;
        unsigned int writable : 1;
        unsigned int closed   : 1;
        unsigned int binary   : 1; /* use fread instead of fgetstr */
    } f;
};

void file_discard(filec_t * file, object_t * obj);
filec_t * file_new(void);
void file_add(filec_t * file);
filec_t * find_file_controller(object_t * obj);
int close_file(filec_t * file);
int flush_file(filec_t * file);
Buffer * read_binary_file(filec_t * file, int block);
string_t * read_file(filec_t * file);
int abort_file(filec_t * file);
int stat_file(filec_t * file, struct stat * sbuf);
string_t * build_path(char * fname, struct stat * sbuf, int nodir);
list_t * statbuf_to_list(struct stat * sbuf);
list_t * open_file(string_t * name, string_t * smode, object_t * obj);
void flush_files(void);
void close_files(void);

#endif

