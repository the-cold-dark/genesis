/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Function operators
//
// This file contains functions inherent to the OS, which are actually
// operators, but nobody needs to know.
//
// Many of these functions require information from the current frame,
// which is why they are not modularized (such as object functions) or
// they are inherent to the functionality of ColdC
//
// The need to split these into seperate files is not too great, as they
// will not be changing often.
*/

#include "defs.h"

#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>  /* func_files() */
#include <fcntl.h>
#include "execute.h"
#include "cache.h"
#include "util.h"      /* some file functions */
#include "file.h"
#include "token.h"     /* is_valid_ident() */

#define GET_FILE_CONTROLLER(__f) { \
        __f = find_file_controller(cur_frame->object); \
        if (__f == NULL) { \
            cthrow(file_id, "No file is bound to this object."); \
            return; \
        } \
    } \

/*
// -----------------------------------------------------------------
*/
void func_fopen(void) {
    cData  * args;
    Int       nargs;
    cList  * stat; 
    filec_t * file;

    if (!func_init_1_or_2(&args, &nargs, STRING, STRING))
        return;

    file = find_file_controller(cur_frame->object);

    /* only one file at a time on an object */
    if (file != NULL) {
        cthrow(file_id, "A file (%s) is already open on this object.",
               file->path->s);
        return;
    }

    /* open the file, it will automagically be set on the current object,
       if we are sucessfull, otherwise our stat list is NULL */
    stat = open_file(args[0].u.str,
                     (nargs == 2 ? args[1].u.str : NULL),
                     cur_frame->object);

    if (stat == NULL)
        return;

    pop(nargs);
    push_list(stat);
    list_discard(stat);
}

/*
// -----------------------------------------------------------------
*/
void func_file(void) {
    filec_t * file;
    cList  * info;
    cData  * list;

    if (!func_init_0())
        return;

    GET_FILE_CONTROLLER(file)

    info = list_new(5);
    list = list_empty_spaces(info, 5);

    list[0].type = INTEGER;
    list[0].u.val = (Long) (file->f.readable ? 1 : 0);
    list[1].type = INTEGER;
    list[1].u.val = (Long) (file->f.writable ? 1 : 0);
    list[2].type = INTEGER;
    list[2].u.val = (Long) (file->f.closed ? 1 : 0);
    list[3].type = INTEGER;
    list[3].u.val = (Long) (file->f.binary ? 1 : 0);
    list[4].type = STRING;
    list[4].u.str = string_dup(file->path);

    push_list(info);
    list_discard(info);
}

/*
// -----------------------------------------------------------------
*/
void func_files(void) {
    cData   * args;
    cStr * path,
             * name;
    cList   * out;
    cData     d;
    struct dirent * dent;
    DIR      * dp;
    struct stat sbuf;

    if (!func_init_1(&args, STRING))
        return;

    path = build_path(args[0].u.str->s, NULL, -1);
    if (!path)
        return;

    if (stat(path->s, &sbuf) == F_FAILURE) {
        cthrow(directory_id, "Unable to find directory \"%s\".", path->s);
        string_discard(path);
        return;
    }

    if (!S_ISDIR(sbuf.st_mode)) {
        cthrow(directory_id, "File \"%s\" is not a directory.", path->s);
        string_discard(path);
        return;
    }

    dp = opendir(path->s);
    out = list_new(0);
    d.type = STRING;
 
    while ((dent = readdir(dp)) != NULL) {
        if (strncmp(dent->d_name, ".", 1) == F_SUCCESS ||
            strncmp(dent->d_name, "..", 2) == F_SUCCESS)
            continue;

#ifdef HAVE_D_NAMLEN
        name = string_from_chars(dent->d_name, dent->d_namlen);
#else
        name = string_from_chars(dent->d_name, strlen(dent->d_name));
#endif
        d.u.str = name;
        out = list_add(out, &d);
        string_discard(name);
    }

    closedir(dp);

    pop(1);
    push_list(out);
    list_discard(out);
}

/*
// -----------------------------------------------------------------
*/
void func_fclose(void) {
    filec_t * file;
    Int       err;

    if (!func_init_0())
        return;

    file = find_file_controller(cur_frame->object);

    if (file == NULL) {
        cthrow(file_id, "A file is not open on this object.");
        return;
    }

    err = close_file(file);
    file_discard(file, cur_frame->object);

    if (err) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// NOTE: args are inverted from the stdio chmod() function call,
// This makes it easier defaulting to this() file.
//
*/
void func_fchmod(void) {
    filec_t * file;
    cData  * args;
    cStr    * path;
    Int       failed,
              nargs;
    Long      mode;
    char    * p,
            * ep;

    if (!func_init_1_or_2(&args, &nargs, STRING, STRING))
        return;

    /* frob the string to a mode_t, somewhat taken from FreeBSD's chmod.c */
    p = args[0].u.str->s;

    SETERR(0);
    mode = strtol(p, &ep, 8);

    if (*p < '0' || *p > '7' || mode > INT_MAX || mode < 0)
        SETERR(ERR_RANGE);
    if (GETERR()) {
        cthrow(file_id, "invalid file mode \"%s\": %s", p, strerror(GETERR()));
        return;
    }

#ifdef RESTRICTIVE_FILES
    /* don't allow SUID mods, incase somebody is being stupid and
       running the server as root; so it could actually happen */
    if (mode & S_ISUID || mode & S_ISGID || mode & S_ISVTX) {
        cthrow(file_id, "You cannot set sticky bits this way, sorry.");
        return;
    }
#endif

    if (nargs == 1) {
        GET_FILE_CONTROLLER(file)
        path = string_dup(file->path);
    } else {
        struct stat sbuf;

        path = build_path(args[1].u.str->s, &sbuf, ALLOW_DIR);
        if (path == NULL)
            return;
    }

    failed = chmod(path->s, mode);
    string_discard(path);

    if (failed) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    pop(2);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_frmdir(void) {
    cData      * args;
    cStr    * path;
    Int           err;
    struct stat   sbuf;

    if (!func_init_1(&args, STRING))
        return;

    path = build_path(args[0].u.str->s, &sbuf, ALLOW_DIR);
    if (!path)
        return;

    err = rmdir(path->s);
    string_discard(path);
    if (err != F_SUCCESS) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fmkdir(void) {
    cData      * args;
    cStr    * path;
    Int           err;
    struct stat   sbuf;

    if (!func_init_1(&args, STRING))
        return;

    path = build_path(args[0].u.str->s, NULL, -1);
    if (!path)
        return;

    if (stat(path->s, &sbuf) == F_SUCCESS) {
        cthrow(file_id,"A file or directory already exists as \"%s\".",path->s);
        string_discard(path);
        return;
    }

    /* default the mode to 0700, they can chmod it later */
    err = mkdir(path->s, 0700);
    string_discard(path);
    if (err != F_SUCCESS) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fremove(void) {
    cData      * args;
    filec_t     * file;
    cStr    * path;
    Int           nargs,
                  err;
    struct stat   sbuf;

    if (!func_init_0_or_1(&args, &nargs, STRING))
        return;

    if (nargs) {
        path = build_path(args[0].u.str->s, &sbuf, DISALLOW_DIR);
        if (!path)
            return;
    } else {
        GET_FILE_CONTROLLER(file)
        path = string_dup(file->path);
    }

    err = unlink(path->s);
    string_discard(path);
    if (err != F_SUCCESS) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    if (nargs)
        pop(1);

    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fseek(void) {
    cData  * args;
    filec_t * file;
    Int       whence = SEEK_CUR;

    if (!func_init_2(&args, INTEGER, SYMBOL))
        return;

    GET_FILE_CONTROLLER(file)
 
    if (!file->f.readable || !file->f.writable) {
        cthrow(file_id,
               "File \"%s\" is not both readable and writable.",
               file->path->s);
        return;
    }

    if (strccmp(ident_name(args[1].u.symbol), "SEEK_SET"))
        whence = SEEK_SET;
    else if (strccmp(ident_name(args[1].u.symbol), "SEEK_CUR")) 
        whence = SEEK_CUR;
    else if (strccmp(ident_name(args[1].u.symbol), "SEEK_END")) 
        whence = SEEK_END;

    if (fseek(file->fp, args[0].u.val, whence) != F_SUCCESS) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    pop(2);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_frename(void) {
    cStr    * from,
                * to;
    cData      * args;
    struct stat   sbuf;
    Int           err,
                  nargs;
    filec_t     * file = NULL;

    if (!func_init_1_or_2(&args, &nargs, 0, STRING))
        return;

    if (args[0].type != STRING) {
        GET_FILE_CONTROLLER(file)
        from = string_dup(file->path);
    } else {
        from = build_path(args[0].u.str->s, &sbuf, ALLOW_DIR);
        if (!from)
            return;
    }

    /* stat it seperately so that we can give a better error */
    to = build_path(args[1].u.str->s, NULL, ALLOW_DIR);
    if (stat(to->s, &sbuf) < 0) {
        cthrow(file_id, "Destination \"%s\" already exists.", to->s);
        string_discard(to);
        string_discard(from);
        return;
    }

    err = rename(from->s, to->s);
    string_discard(from);
    string_discard(to);
    if (err == F_SUCCESS) {
        if (file) {
            string_discard(file->path);
            file->path = string_dup(to);
        }
    } else {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    pop(nargs);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fflush(void) {
    filec_t * file;

    if (!func_init_0())
        return;

    GET_FILE_CONTROLLER(file)

    if (fflush(file->fp) == EOF) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_feof(void) {
    filec_t * file;

    if (!func_init_0())
        return;

    GET_FILE_CONTROLLER(file);

    if (feof(file->fp))
        push_int(1);
    else
        push_int(0);
}

/*
// -----------------------------------------------------------------
*/
void func_fread(void) {
    cData  * args;
    Int       nargs;
    filec_t  * file;

    if (!func_init_0_or_1(&args, &nargs, INTEGER))
        return;

    GET_FILE_CONTROLLER(file)

    if (!file->f.readable) {
        cthrow(file_id, "File is not readable.");
        return;
    }

    if (file->f.binary) {
        cBuf * buf = NULL;
        Int      block = DEF_BLOCKSIZE;

        if (nargs) {
            block = args[0].u.val;
            pop(1);
        }

        buf = read_binary_file(file, block);
   
        if (!buf)
            return;

        push_buffer(buf);
        buffer_discard(buf);
    } else {
        cStr * str = read_file(file);

        if (!str)
            return;

        if (nargs)
            pop(1);

        push_string(str);
        string_discard(str);
    }
}

/*
// -----------------------------------------------------------------
*/
void func_fwrite(void) {
    cData   * args;
    Int        count;
    filec_t  * file;

    if (!func_init_1(&args, 0))
        return;

    GET_FILE_CONTROLLER(file)

    if (!file->f.writable) {
        cthrow(perm_id, "File is not writable.");
        return;
    }

    if (file->f.binary) {
        if (args[0].type != BUFFER) {
            cthrow(type_id,"File type is binary, you may only fwrite buffers.");
            return;
        }
        count = fwrite(args[0].u.buffer->s,
                       sizeof(unsigned char),
                       args[0].u.buffer->len,
                       file->fp);
        count -= args[0].u.buffer->len;
    } else {
        if (args[0].type != STRING) {
            cthrow(type_id, "File type is text, you may only fwrite strings.");
            return;
        }
        count = fwrite(args[0].u.str->s,
                       sizeof(unsigned char),
                       args[0].u.str->len,
                       file->fp);
        count -= args[0].u.str->len;

        /* if we successfully wrote everything, drop a newline on it */
        if (!count)
            fputc((char) 10, file->fp);
    }

    pop(1);
    push_int(count);
}

/*
// -----------------------------------------------------------------
*/
void func_fstat(void) {
    struct stat    sbuf;
    cList       * stat;
    cData       * args;
    Int            nargs;
    filec_t      * file;

    if (!func_init_0_or_1(&args, &nargs, STRING))
        return;

    if (!nargs) {
        GET_FILE_CONTROLLER(file)
        stat_file(file, &sbuf);
    } else {
        cStr * path = build_path(args[0].u.str->s, &sbuf, ALLOW_DIR);

        if (!path)
            return;

        string_discard(path);
    }

    stat = statbuf_to_list(&sbuf);

    push_list(stat);
    list_discard(stat);
}   


/*
// -----------------------------------------------------------------
//
// run an executable from the filesystem
//
*/

void func_execute(void) {
    cData *args, *d;
    cList *script_args;
    Int num_args, argc, len, i, fd, dlen;
    int status;
    pid_t pid;
    char *fname, **argv;

    /* Accept a name of a script to run, a list of arguments to give it, and
     * an optional flag signifying that we should not wait for completion. */
    if (!func_init_2_or_3(&args, &num_args, STRING, LIST, INTEGER))
        return;

    script_args = args[1].u.list;

    /* Verify that all items in argument list are strings. */
    for (d = list_first(script_args), i=0;
         d;
         d = list_next(script_args, d), i++) {
        if (d->type != STRING) {
            cthrow(type_id,
                   "Execute argument %d (%D) is not a string.",
                   i+1, d);
            return;
        }
    }

    /* Don't allow walking back up the directory tree. */
    if (strstr(string_chars(args[0].u.str), "../")) {
        cthrow(perm_id, "Filename %D is not legal.", &args[0]);
        return;
    }

    /* Construct the name of the script. */
    len = string_length(args[0].u.str);
    dlen = strlen(c_dir_bin);

    /* +2 is 1 for '/' and 1 for NULL */
    fname = TMALLOC(char, len + dlen + 2);
    memcpy(fname, c_dir_bin, dlen);
    fname[dlen] = '/';
    memcpy(fname + dlen + 1, string_chars(args[0].u.str), len);
    fname[len + dlen + 1] = (char) NULL;

    /* Build an argument list. */
    argc = list_length(script_args) + 1;
    argv = TMALLOC(char *, argc + 1);
    argv[0] = tstrdup(fname);
    for (d = list_first(script_args), i = 0;
         d;
         d = list_next(script_args, d), i++)
        argv[i + 1] = tstrdup(string_chars(d->u.str));
    argv[argc] = NULL;

    pop(num_args);

    /* Fork off a process. */
#ifdef USE_VFORK
    pid = vfork();
#else
    pid = fork();
#endif
    if (pid == 0) {
        /* Pipe stdin and stdout to /dev/null, keep stderr. */
        fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            write_err("EXEC: Failed to open /dev/null: %s.",strerror(GETERR()));
            _exit(-1);
        }
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        execv(fname, argv);
        write_err("EXEC: Failed to exec \"%s\": %s.", fname,strerror(GETERR()));
        _exit(-1);
    } else if (pid > 0) {
        if (num_args == 3 && args[2].u.val) {
            if (waitpid(pid, &status, WNOHANG) == 0)
                status = 0;
        } else {
            waitpid(pid, &status, 0);
        }
    } else {
        write_err("EXEC: Failed to fork: %s.", strerror(GETERR()));
        status = -1;
    }

    /* Free the argument list. */
    for (i = 0; i < argc; i++)
        tfree_chars(argv[i]);
    TFREE(argv, argc + 1);

    push_int(status);
}

