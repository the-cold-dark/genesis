/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: genesis.c
// ---
//
*/

#define _main_

#include "config.h"
#include "defs.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "codegen.h"
#include "opcodes.h"
#include "cdc_types.h"
#include "object.h"
#include "data.h"
#include "ident.h"
#include "cdc_string.h"
#include "match.h"
#include "cache.h"
#include "sig.h"
#include "binarydb.h"
#include "util.h"
#include "io.h"
#include "file.h"
#include "log.h"
#include "execute.h"
#include "token.h"
#include "native.h"
#include "memory.h"

INTERNAL void initialize(int argc, char **argv);
INTERNAL void main_loop(void);
void usage (char * name);

/*
// --------------------------------------------------------------------
//
// Rather obvious, no?
//
*/

int main(int argc, char **argv) {
    /* make us look purdy */

    initialize(argc, argv);
    main_loop();

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

INTERNAL void initialize(int argc, char **argv) {
    list_t   * args;
    string_t * str;
    data_t     arg;
    char     * opt,
             * name,
             * basedir = NULL,
             * buf;
    FILE     * fp;
    int        dofork = 1;
    pid_t      pid;

    name = *argv;
    argv++;
    argc--;

    /* Ditch stdin, so we can reuse the file descriptor */
    fclose(stdin);

    init_defs();

    /* Initialize internal tables and variables. */
    init_codegen();
    init_ident();
    init_op_table();
    init_match();
    init_util();
    init_sig();
    init_execute();
    init_scratch_file();
    init_token();
    init_modules(argc, argv);

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
                        NEWFILE(c_pidfile, buf);
                        break;
                    case 'f':
                        dofork = 0;
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
                c_logfile, strerror(errno));
        logfile = stdout;
    }

    if (strccmp(c_errfile, "stdout") == 0)
        logfile = stdout;
    else if (strccmp(c_errfile, "stderr") != 0 &&
             (errfile = fopen(c_errfile, "ab")) == NULL)
    {
        fprintf(stderr, "Unable to open log %s: %s\nDefaulting to stderr..\n",
                c_errfile, strerror(errno));
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
                fprintf(stderr,"genesis: unable to fork: %s\n",strerror(errno));
            exit(0);
        }
    }

    /* print the PID */
    if ((fp = fopen(c_pidfile, "wb")) != NULL) {
        fprintf(fp, "%ld\n", (long) getpid());
        fclose(fp);
    }

    /* Initialize database and network modules. */
    init_cache();
    init_binary_db();
    init_core_objects();

    /* give useful information, good for debugging */
    { /* reduce the scope */
        data_t   * d;
        string_t * str;
        int        first = 1;

        fputs("Calling $sys.startup(", errfile);
        for (d=list_first(args); d; d=list_next(args, d)) {
            str = data_to_literal(d);
            if (!first) {
                fputc(',', errfile);
                fputc(' ', errfile);
            } else {
                first = 0;
            }
            fputs(string_chars(str), errfile);
            string_discard(str);
        }
        fputs(")...\n", errfile);
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
*/

INTERNAL void main_loop(void) {
    int             seconds;
    time_t          next_heartbeat,
                    last_heartbeat,
                    t;

    setjmp(main_jmp);

    seconds = 0;
    next_heartbeat = last_heartbeat = t = 0;

    while (running) {
	/* delete any defunct connection or server records */
	flush_defunct();

	/* Sanity check: make sure there are no objects in active chains. */
	/* cache_sanity_check(); */

	/* Find number of seconds before next heartbeat. */
	if (heartbeat_freq != -1) {
	    next_heartbeat = (last_heartbeat -
			      (last_heartbeat % heartbeat_freq)
			      ) + heartbeat_freq;
	    time(&t);
	    seconds = (t >= next_heartbeat) ? 0 : next_heartbeat - t;
	    seconds = (preempted ? 0 : seconds);
	}

        /* wait seconds for something to happen */
        handle_io_event_wait(seconds);

        /* input */
        handle_connection_input();

        /* handle new or pending connections */
	handle_new_and_pending_connections();

        /* do heartbeat? */
	if (heartbeat_freq != -1) {
	    time(&t);

            /* yep */
	    if (t >= next_heartbeat) {
                /* call heartbeat on $sys */
		last_heartbeat = t;
		task(SYSTEM_OBJNUM, heartbeat_id, 0);

#ifdef CLEAN_CACHE
                /* cleanup the cache while we are at it */
                cache_cleanup();
#endif
	    }
	}

        /* output */
        handle_connection_output();

        /* complete paused tasks */
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
    -v               version.\n\
    -b binary        binary database directory name, default: \"%s\"\n\
    -r root          root file directory name, default: \"%s\"\n\
    -x bindir        db executables directory, default: \"%s\"\n\
    -d log           alternate database logfile, default: \"%s\"\n\
    -e log           alternate error (driver) file, default: \"%s\"\n\
    -p pidfile       alternate pid file, default: \"%s\"\n\
    -f               do not fork on startup\n\
\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, name, c_dir_binary,
     c_dir_root, c_dir_bin, c_logfile, c_errfile, c_pidfile);
}

#undef _main_
