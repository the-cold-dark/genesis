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

#ifdef __Win32__
#define STDIN_FILENO (fileno(stdin))
#define STDOUT_FILENO (fileno(stdout))
#define STDERR_FILENO (fileno(stderr))
#endif

#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __UNIX__
#include <sys/wait.h>
#endif

#include <dirent.h>  /* func_files() */
#ifdef __MSVC__
#include <direct.h>
#include <io.h>
#endif

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
COLDC_FUNC(fopen) {
    cList  * stat; 
    filec_t * file;

    INIT_1_OR_2_ARGS(STRING, STRING);

    file = find_file_controller(cur_frame->object);

    /* only one file at a time on an object */
    if (file != NULL) {
        cthrow(file_id, "A file (%s) is already open on this object.",
               file->path->s);
        return;
    }

    /* open the file, it will automagically be set on the current object,
       if we are sucessfull, otherwise our stat list is NULL */
    stat = open_file(STR1, (argc == 2 ? STR2 : NULL), cur_frame->object);

    /* if its null, open_file() threw an error */
    if (stat == NULL)
        return;

    pop(argc);
    push_list(stat);
    list_discard(stat);
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(file) {
    filec_t * file;
    cList  * info;
    cData  * list;

    INIT_NO_ARGS();

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
COLDC_FUNC(files) {
    cStr          * path,
                  * name;
    cList         * out;
    cData           d;
    struct dirent * dent;
    DIR           * dp;
    struct stat     sbuf;

    INIT_1_ARG(STRING);

    path = build_path(STR1->s, NULL, -1);
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

    if ((dp = opendir(path->s)) == NULL) {
        cthrow(directory_id, "opendir(%s): %s", path->s, strerror(errno));
        string_discard(path);
        return;
    }
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
    string_discard(path);
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(fclose) {
    filec_t * file;
    Int       err;

    INIT_NO_ARGS();

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
COLDC_FUNC(fchmod) {
    filec_t * file;
    cStr    * path;
    Int       failed;
    Long      mode;
    char    * p,
            * ep;

    INIT_1_OR_2_ARGS(STRING, STRING);

    /* frob the string to a mode_t */
    p = STR1->s;

    /* strtol sets an error if an overflow/underflow occurs */
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

    if (argc == 1) {
        GET_FILE_CONTROLLER(file)
        path = string_dup(file->path);
    } else {
        struct stat sbuf;

        path = build_path(STR2->s, &sbuf, ALLOW_DIR);
        if (path == NULL)
            return;
    }

#ifdef __MSVC__
    failed = _chmod(path->s, mode);
#else
    failed = chmod(path->s, mode);
#endif
    string_discard(path);

    if (failed) {
        cthrow(file_id, strerror(GETERR()));
        return;
    }

    pop(argc);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(frmdir) {
    cStr        * path;
    Int           err;
    struct stat   sbuf;

    INIT_1_ARG(STRING);

    if (!(path = build_path(STR1->s, &sbuf, ALLOW_DIR)))
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
COLDC_FUNC(fmkdir) {
    cStr        * path;
    Int           err;
    struct stat   sbuf;

    INIT_1_ARG(STRING);

    if (!(path = build_path(args[0].u.str->s, NULL, -1)))
        return;

    if (stat(path->s, &sbuf) == F_SUCCESS) {
        cthrow(file_id,"A file or directory already exists as \"%s\".",path->s);
        string_discard(path);
        return;
    }

    /* default the mode to 0700, they can chmod it later */
#ifdef __MSVC__
    err = mkdir(path->s);
#else
    err = mkdir(path->s, 0700);
#endif
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
COLDC_FUNC(fremove) {
    cStr        * path;
    Int           err;
    struct stat   sbuf;

    INIT_1_ARG(STRING);

    path = build_path(STR1->s, &sbuf, DISALLOW_DIR);
    if (!path)
        return;

    err = unlink(path->s);

    string_discard(path);

    if (err != F_SUCCESS)
        THROW((file_id, strerror(GETERR())))

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(fseek) {
    filec_t * file;
    Int       whence;

    INIT_2_ARGS(INTEGER, SYMBOL);

    GET_FILE_CONTROLLER(file)
 
    if (!file->f.readable || !file->f.writable)
        THROW((file_id,
               "File \"%s\" is not both readable and writable.",
               file->path->s))

    if (SYM2 == SEEK_SET_id)
        whence = SEEK_SET;
    else if (SYM2 == SEEK_CUR_id)
        whence = SEEK_CUR;
    else if (SYM2 == SEEK_END_id)
        whence = SEEK_END;
    else
        THROW((type_id,"Whence is not one of 'SEEK_SET 'SEEK_CUR or 'SEEK_END"))

    if (fseek(file->fp, (long) INT1, whence) != F_SUCCESS)
        THROW((file_id, strerror(GETERR())))

    pop(2);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(frename) {
    cStr        * from,
                * to;
    struct stat   sbuf;
    Int           err;
    filec_t     * file = NULL;

    INIT_2_ARGS(ANY_TYPE, STRING);

    if (args[0].type != STRING || !string_length(STR1)) {
        GET_FILE_CONTROLLER(file)
        from = string_dup(file->path);
    } else if (!(from = build_path(args[0].u.str->s, &sbuf, ALLOW_DIR)))
        return;

    /* stat it seperately so that we can give a better error */
    to = build_path(STR2->s, NULL, ALLOW_DIR);
    if (stat(to->s, &sbuf) == 0) {
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

    pop(2);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(fflush) {
    filec_t * file;

    INIT_NO_ARGS();

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
COLDC_FUNC(feof) {
    filec_t * file;

    INIT_NO_ARGS();

    GET_FILE_CONTROLLER(file);

    if (feof(file->fp))
        push_int(1);
    else
        push_int(0);
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(fread) {
    filec_t  * file;

    INIT_0_OR_1_ARGS(INTEGER);

    GET_FILE_CONTROLLER(file)

    if (!file->f.readable) {
        cthrow(file_id, "File is not readable.");
        return;
    }

    if (file->f.binary) {
        cBuf * buf = NULL;
        Int      block = DEF_BLOCKSIZE;

        if (argc) {
            block = INT1;
            pop(1);
        }

        buf = read_binary_file(file, block);
   
        if (!buf)
            return;

        push_buffer(buf);
        buffer_discard(buf);
    } else {
        cStr * str = read_file(file);

        /* if its null, read_file() threw an error .. */
        if (!str)
            return;

        if (argc)
            pop(1);

        push_string(str);
        string_discard(str);
    }
}

/*
// -----------------------------------------------------------------
*/
COLDC_FUNC(fwrite) {
    Int        count;
    filec_t  * file;

    INIT_1_ARG(ANY_TYPE);

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
COLDC_FUNC(fstat) {
    struct stat    sbuf;
    cList        * stat;
    filec_t      * file;

    INIT_0_OR_1_ARGS(STRING);

    if (!argc) {
        GET_FILE_CONTROLLER(file)
        stat_file(file, &sbuf);
    } else {
        cStr * path = build_path(STR1->s, &sbuf, ALLOW_DIR);

        /* if path == NULL build_path() threw an error */
        if (!path)
            return;

        string_discard(path);
    }

    stat = statbuf_to_list(&sbuf);

    /* don't call pop unless we need to */
    if (argc)
        pop(1);

    push_list(stat);
    list_discard(stat);
}   


/*
// -----------------------------------------------------------------
//
// run an executable from the filesystem
//
*/

COLDC_FUNC(execute) {
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

#ifdef __Win32__

#ifndef CREATE_PROCESS
    write_err("EXEC: No forking in Win32 (yet)");
    /* for now this will not work - JAL */
    status = -1;

#else

    fSuccess = CreateProcess((LPTSTR)fname, (LPTSTR)argv,
                   (LPSECURITY_ATTRIBUTES)NULL, (LPSECURITY_ATTRIBUTES)NULL,
                   (BOOL)TRUE,(DWORD)0, NULL, NULL, (LPSTARTUPINFO)&SI,
                   (LPPROCESS_INFORMATION)&pi);
    /* parent waits for child */
    if (fSuccess) {
        hProcess = pi.hProcess;    hThread = pi.hThread;
        dw = WaitForSingleObject(hProcess, INFINITE);
        /* if we saw success ... */
        if (dw != 0xFFFFFFFF) {
                /* pick up an exit code for the process */
                fExit = GetExitCodeProcess(hProcess, &dwExitCode);
        }
        /* close the child process and thread object handles */
        CloseHandle(hThread);    CloseHandle(hProcess);
        printf("COMPLETED!\n");
    } else
        printf("Failed to CreateProcess\n");

#endif

#else

    /* Fork off a process. */
    pid = FORK_PROCESS();
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
            /* on at least Solaris, waitpid() won't write 0 
             * into &status. only non-zero values
             */
            status = 0;
            if (waitpid(pid, &status, WNOHANG) == 0)
                status = 0;
        } else {
            status = 0;
            waitpid(pid, &status, 0);
        }
    } else {
        write_err("EXEC: Failed to fork: %s.", strerror(GETERR()));
        status = -1;
    }

#endif

    /* Free the argument list. */
    for (i = 0; i < argc; i++)
        tfree_chars(argv[i]);
    TFREE(argv, argc + 1);

    push_int(status);
}
