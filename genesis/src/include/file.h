/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_file_h
#define cdc_file_h

typedef struct filec_s       filec_t;

#include <sys/types.h>
#include <sys/stat.h>

#define DISALLOW_DIR 1
#define ALLOW_DIR 0

struct filec_s {
    FILE     * fp;
    cObjnum   objnum;
    filec_t  * next;
    cStr * path;
    struct {
        unsigned int readable : 1;
        unsigned int writable : 1;
        unsigned int closed   : 1;
        unsigned int binary   : 1; /* use fread instead of fgetstr */
    } f;
};

void file_discard(filec_t * file, Obj * obj);
filec_t * file_new(void);
void file_add(filec_t * file);
filec_t * find_file_controller(Obj * obj);
Int close_file(filec_t * file);
Int flush_file(filec_t * file);
cBuf * read_binary_file(filec_t * file, Int block);
cStr * read_file(filec_t * file);
Int abort_file(filec_t * file);
Int stat_file(filec_t * file, struct stat * sbuf);
cStr * build_path(char * fname, struct stat * sbuf, Int nodir);
cList * statbuf_to_list(struct stat * sbuf);
cList * open_file(cStr * name, cStr * smode, Obj * obj);
void flush_files(void);
void close_files(void);

#endif

