/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Main file for 'genesis' executable
*/

#define _main_

#include "defs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>
#include <time.h>
#include "cdc_pcode.h"
#include "cdc_db.h"
#include "strutil.h"
#include "util.h"
#include "file.h"
#include "net.h"
#include "sig.h"

INTERNAL void initialize(Int argc, char **argv);
INTERNAL void main_loop(void);

void usage (char * name);

/*
// if we have a logs/genesis.run, unlink it when we exit,
// hooked into exiting with 'atexit()'
*/
void unlink_runningfile(void) {
    if (unlink(c_runfile)) {
        char buf[BUF];

        /* grasp! */
        strcpy(buf, "rm -f ");
        strcat(buf, c_runfile);
        system(buf);
    }
}

/*
// --------------------------------------------------------------------
//
// The big kahuna
//
*/

int main(int argc, char **argv) {
    /* make us look purdy */

    initialize((Int) argc, argv);
    main_loop();

#ifdef PROFILE_EXECUTE
   dump_execute_profile();
#endif

    /* Sync the cache, flush output buffers, and exit normally. */
    cache_sync();
    db_close();
    flush_output();
    close_files();

    return 0;
}

/*
// --------------------------------------------------------------------
//
// Initialization
//
*/
void get_my_hostname(void) {
    FILE * cp = popen("(hostname || uname -n) 2>/dev/null", "r");
    char   cbuf[LINE];
    char * s,
         * e;

    if (!cp)
        fprintf(stderr, "Unable to determine hostname.\n");
    else {
        fgets(cbuf, LINE, cp);
        s = cbuf;
        while (isspace(*s))
            s++;
        e = &s[strlen(s)-1];
        while (isspace(*e))
            e--;
        *(e+1) = (char) NULL;
        if (strlen(s)) {
            string_discard(str_hostname);
            str_hostname = string_from_chars(s, strlen(s));
        } else
            fprintf(stderr, "Unable to determine hostname.\n");
    }
}

#define addarg(__str) { \
        arg.type = STRING; \
        str = string_from_chars(__str, strlen(__str)); \
        arg.u.str = str; \
        args = list_add(args, &arg); \
        string_discard(str); \
    } \

#define NEWFILE(var, name) { \
        free(var); \
        var = EMALLOC(char, strlen(name) + 1); \
        strcpy(var, name); \
    }

INTERNAL void initialize(Int argc, char **argv) {
    cList   * args;
    cStr     * str;
    cData     arg;
    char     * opt,
             * name,
             * basedir = NULL,
             * buf;
    FILE     * fp;
    Bool       dofork = YES;
    pid_t      pid;

    name = *argv;
    argv++;
    argc--;

    /* Ditch stdin, so we can reuse the file descriptor */
    fclose(stdin);

    /* basic initialization */
    init_defs();
    init_match();
    init_util();

    /* db argument list */
    args = list_new(0);

    /* parse arguments */
    while (argc) {
        opt = *argv;
        if (*opt == '-') {
            opt++;
            if (*opt == '-') {
                addarg(opt);
            } else {
                switch (*opt) {
                    case 'b':
                        argv += getarg(name,&buf,opt,argv,&argc,usage);
                        NEWFILE(c_dir_binary, buf);
                        break;
                    case 'r':
                        argv += getarg(name,&buf,opt,argv,&argc,usage);
                        NEWFILE(c_dir_root, buf);
                        break;
                    case 'x':
                        argv += getarg(name,&buf,opt,argv,&argc,usage);
                        NEWFILE(c_dir_bin, buf);
                        break;
                    case 'd':
                        argv += getarg(name,&buf,opt,argv,&argc,usage);
                        NEWFILE(c_logfile, buf);
                        break;
                    case 'e':
                        argv += getarg(name,&buf,opt,argv,&argc,usage);
                        NEWFILE(c_errfile, buf);
                        break;
                    case 'p':
                        argv += getarg(name,&buf,opt,argv,&argc,usage);
                        NEWFILE(c_runfile, buf);
                        break;
                    case 'n':
                        argv += getarg(name,&buf,opt,argv,&argc,usage);
                        if (buf && strlen(buf)) {
                            string_discard(str_hostname);
                            str_hostname = string_from_chars(buf, strlen(buf));
                        }
                        break;
                    case 's': {
                        char * p;
    
                        argv += getarg(name, &buf, opt, argv, &argc, usage);
                        p = buf;
                        cache_width = atoi(p);
                        while (*p && isdigit(*p))
                            p++;
                        if (LCASE(*p) == 'x') {
                            p++;
                            cache_depth = atoi(p);
                        } else {
                            usage(name);
                            printf("\n** Invalid WIDTHxDEPTH: '%s'\n", buf);
                            exit(0);
                        }
                        if (cache_width == 0 && cache_depth == 0) {
                            usage(name);
                         puts("\n** The WIDTH and DEPTH cannot both be zero\n");
                            exit(0);
                        }
                        break;
                    }
                    case 'f':
                        dofork = NO;
                        break;
                    case 'v':
                        printf("Genesis %d.%d-%d\n",
                               VERSION_MAJOR,
                               VERSION_MINOR,
                               VERSION_PATCH);
                        exit(0);
                        break;
                    case 'h':
                        usage(name);
                        exit(0);
                        break;
                    default:
                        usage(name);
                        fprintf(stderr, "** Invalid argument -%s\n** send arguments to the database by prefixing them with '--', not '-'\n", opt); 
                        exit(1);
                }
            }
        } else {
            if (basedir == NULL)
                basedir = *argv;
            else
                addarg(*argv);
        }
        argv++;
        argc--;
    }

    /* Initialize internal tables and variables. */
#ifdef DRIVER_DEBUG
    init_debug();
#endif
    init_codegen();
    init_ident();
    init_op_table();
    init_sig();
    init_execute();
    init_scratch_file();
    init_token();
    init_modules(argc, argv);
    init_net();

    /* Figure out our hostname */
    if (!string_length(str_hostname))
        get_my_hostname();

    /* where is the base db directory ? */
    if (basedir == NULL)
        basedir = ".";

    /* Switch into database directory. */
    if (chdir(basedir) == F_FAILURE) {
        usage(name);
        fprintf(stderr, "** Couldn't change to base directory \"%s\".\n",
                basedir);
        exit(1);
    }

    /* open the correct logfiles */
    if (strccmp(c_logfile, "stderr") == 0)
        logfile = stderr;
    else if (strccmp(c_logfile, "stdout") != 0 &&
             (logfile = fopen(c_logfile, "ab")) == NULL)
    {
        fprintf(stderr, "Unable to open log %s: %s\nDefaulting to stdout..\n",
                c_logfile, strerror(GETERR()));
        logfile = stdout;
    }

    if (strccmp(c_errfile, "stdout") == 0)
        logfile = stdout;
    else if (strccmp(c_errfile, "stderr") != 0 &&
             (errfile = fopen(c_errfile, "ab")) == NULL)
    {
        fprintf(stderr, "Unable to open log %s: %s\nDefaulting to stderr..\n",
                c_errfile, strerror(GETERR()));
        errfile = stderr;
    }

    /* fork ? */
    if (dofork) {
#ifdef USE_VFORK
        pid = vfork();
#else
        pid = fork();
#endif
        if (pid != 0) { /* parent */
            if (pid == -1)
                fprintf(stderr,"genesis: unable to fork: %s\n",strerror(GETERR()));
            exit(0);
        }
    }

    /* print the PID */
    if ((fp = fopen(c_runfile, "wb")) != NULL) {
        fprintf(fp, "%ld\n", (long) getpid());
        fclose(fp);
        atexit(unlink_runningfile);
    } else {
        fprintf(errfile, "genesis pid: %ld\n", (long) getpid());
    }

    /* Initialize database and network modules. */
    init_cache();
    init_binary_db();
    init_core_objects();

    /* give useful information, good for debugging */
    { /* reduce the scope */
        cData   * d;
        cStr     * str;
        Bool       first = YES;

        fputs("Calling $sys.startup([", errfile);
        for (d=list_first(args); d; d=list_next(args, d)) {
            str = data_to_literal(d, TRUE);
            if (!first) {
                fputc(',', errfile);
                fputc(' ', errfile);
            } else {
                first = NO;
            }
            fputs(string_chars(str), errfile);
            string_discard(str);
        }
        fputs("])...\n", errfile);
    }

    /* call $sys.startup() */
    arg.type = LIST;
    arg.u.list = args;
    task(SYSTEM_OBJNUM, startup_id, 1, &arg);
    list_discard(args);
}

/*
// --------------------------------------------------------------------
//
// The core of the interpreter, while this is looping it is interpreting
//
// use 'gettimeofday' since most OS's simply wrap time() around it
*/

INTERNAL void main_loop(void) {
    register Int     seconds;
    register time_t  next, last;

#ifdef __Win32__
    time_t           tm;

#define SECS tm
#define GETTIME() time(&tm)

#else
    /* Unix--most unix systems wrap time() around gettimeofday() */
    struct timeval   tp;

#define SECS tp.tv_sec
#define GETTIME() gettimeofday(&tp, NULL)
#endif

    setjmp(main_jmp);

    seconds = 0;
    next = last = 0;

    while (running) {
        flush_defunct();

        /* cache_sanity_check(); */

        /* determine io wait */
        if (heartbeat_freq != -1) {
            next = (last - (last % heartbeat_freq)) + heartbeat_freq;
            GETTIME();
            seconds = (preempted ? 0 :
                       ((SECS >= next) ? 0 : next - SECS));
        }

        /* push our dump along, diddle with the wait if we need to */
        switch (dump_some_blocks(DUMP_BLOCK_SIZE)) {
            case DUMP_FINISHED:
                finish_backup();
                task(SYSTEM_OBJNUM, backup_done_id, 0);
                break;
            case DUMP_DUMPED_BLOCKS:
                seconds = 0; /* we are still dumping, dont wait */
                break;
        }

        handle_io_event_wait(seconds);
        handle_connection_input();
        handle_new_and_pending_connections();

        if (heartbeat_freq != -1) {
            GETTIME();
            if (SECS >= next) {
                last = SECS;
                task(SYSTEM_OBJNUM, heartbeat_id, 0);
#ifdef CLEAN_CACHE
                cache_cleanup();
#endif
            }
        }

        handle_connection_output();
        if (preempted)
            run_paused_tasks();
    }
}

void usage (char * name) {
    fprintf(stderr, "\n-- Genesis %d.%d-%d --\n\n\
Usage: %s [base dir] [options]\n\n\
    Base directory will default to \".\" if unspecified.  Arguments which\n\
    the driver does not recognize, or options which begin with \"--\" rather\n\
    than \"-\" are passed onto the database as arguments to $sys.startup().\n\n\
    Note: specifying \"stdin\" or \"stderr\" for either of the logs will\n\
    direct them appropriately.\n\n\
Options:\n\n\
        -v             version.\n\
        -b binary      binary database directory name, default: \"%s\"\n\
        -r root        root file directory name, default: \"%s\"\n\
        -x bindir      db executables directory, default: \"%s\"\n\
        -d log         alternate database logfile, default: \"%s\"\n\
        -e log         alternate error (driver) file, default: \"%s\"\n\
        -p file        alternate runtime pid file, default: \"%s\"\n\
        -f             do not fork on startup\n\
        -s WIDTHxDEPTH Cache size, default 10x30\n\
\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, name, c_dir_binary,
     c_dir_root, c_dir_bin, c_logfile, c_errfile, c_runfile);
}

/* TEMPORARY-- we need an area where identical functions 'names' (yet
   different behaviours) between genesis/coldcc exist */

cObjnum get_object_name(Ident id) {
    cObjnum num;

    if (!lookup_retrieve_name(id, &num))
        num = db_top++;
    return num;
}

#undef _main_
