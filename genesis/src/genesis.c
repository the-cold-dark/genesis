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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "config.h"
#include "defs.h"
#include "y.tab.h"
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
#include "db.h"
#include "util.h"
#include "io.h"
#include "log.h"
#include "dump.h"
#include "execute.h"
#include "token.h"
#include "modules.h"

internal void initialize(int argc, char **argv);
internal void main_loop(void);
internal void usage (char * name);

/*
// --------------------------------------------------------------------
//
// Rather obvious, no?
//
*/

int main(int argc, char **argv) {
    initialize(argc, argv);
    main_loop();

    /* Sync the cache, flush output buffers, and exit normally. */
    cache_sync();
    db_close();
    flush_output();

    return 0;
}

/*
// --------------------------------------------------------------------
//
// Initialization
//
*/

#define nextarg() { \
        argv++; \
        argc--; \
        if (!argc) { \
            usage(name); \
            fprintf(stderr, "** No followup argument to -%s.\n", opt); \
            exit(0); \
        } \
    }

#define addarg(__str) { \
        arg.type = STRING; \
        str = string_from_chars(__str, strlen(__str)); \
	arg.u.str = str; \
        args = list_add(args, &arg); \
        string_discard(str); \
    } \

internal void initialize(int argc, char **argv) {
    object_t * obj;
    list_t   * parents,
             * args;
    string_t * str;
    data_t     arg,
             * d;
    char     * opt,
             * name,
             * basedir = NULL;

    name = *argv;
    argv++;
    argc--;

    /* Ditch stdin, so we can reuse the file descriptor */
    fclose(stdin);

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
    init_defs();

    /* db argument list */
    args = list_new(0);

    /* parse arguments */
    while (argc) {
        opt = *argv;
        if (*opt == '-') {
            if (strlen(opt) != 2) {
                addarg(*argv);
            } else {
                opt++;
                switch (*opt) {
                    case 'b':
                        nextarg();
                        strcpy(c_dir_binary, *argv);
                        break;
                    case 'r':
                        nextarg();
                        strcpy(c_dir_root, *argv);
                        break;
                    case 't':
                        nextarg();
                        strcpy(c_dir_textdump, *argv);
                        break;
                    case 'e':
                        nextarg();
                        strcpy(c_dir_bin, *argv);
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
                        addarg(*argv);
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

    /* people like to know what is up */
    fprintf(stderr, "Initializing database...");

    /* Initialize database and network modules. */
    init_cache();
#if 1
    init_binary_db();
#else
    use_textdump = init_db(0);
#endif

    /* Order of operations note: it might seem like we'd want to read the text
     * dump (if we're going to) before making sure there's a root and system
     * object.  However, this way is correct, since the textdump reader can
     * evaluate arbitrary ColdC code and thus should start with a consistent
     * database. */

    /* Make sure there is a root object. */
    obj = cache_retrieve(ROOT_DBREF);
    if (!obj) {
	parents = list_new(0);
	obj = object_new(ROOT_DBREF, parents);
	list_discard(parents);
    }
    cache_discard(obj);

    /* Make sure there is a system object. */
    obj = cache_retrieve(SYSTEM_DBREF);
    if (!obj) {
	parents = list_new(1);
	d = list_empty_spaces(parents, 1);
	d->type = DBREF;
	d->u.dbref = ROOT_DBREF;
	obj = object_new(SYSTEM_DBREF, parents);
	list_discard(parents);
    }
    cache_discard(obj);

#if DISABLED
    /* Read a text dump if there was no existing binary database. */
    if (use_textdump) {
        write_err("Reading from textdump...");
	fp = fopen("textdump", "r");
	if (!fp) {
	    fail_to_start("Couldn't open text dump file.");
	} else {
	    text_dump_read(fp);
	    fclose(fp);
	}
    }
#endif

    printf("Sending Startup Message.\n");
    /* Send a startup message to the system object. */
    arg.type = LIST;
    arg.u.list = args;
    task(NULL, SYSTEM_DBREF, startup_id, 1, &arg);
    list_discard(args);
}

/*
// --------------------------------------------------------------------
//
// The core of the interpreter, while this is looping it is interpreting
//
*/

internal void main_loop(void) {
    int             seconds = 0;
    time_t          next_heartbeat = 0,
                    last_heartbeat = 0,
                    t = 0;

    while (running) {
	/* delete any defunct connection or server records */
	flush_defunct();

	/* Sanity check: make sure there are no objects in active chains. */
	/*	cache_sanity_check(); */

	/* Find number of seconds before next heartbeat. */
	if (heartbeat_freq != -1) {
	    next_heartbeat = (last_heartbeat -
			      (last_heartbeat % heartbeat_freq)
			      ) + heartbeat_freq;
	    time(&t);
	    seconds = (t >= next_heartbeat) ? 0 : next_heartbeat - t;
	    seconds = (paused ? 0 : seconds);
            /* fprintf(stderr, "seconds: %d\n", seconds); */
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
		task(NULL, SYSTEM_DBREF, heartbeat_id, 0);

                /* clenup the cache */
                cache_cleanup();
	    }
	}

        /* output */
        handle_connection_output();

        /* complete paused tasks */
	if (paused)
            run_paused_tasks();
    }
}

internal void usage (char * name) {
    fprintf(stderr, "\n-- Genesis %d.%d-%d --\n\n\
Usage: %s [base dir] [options]\n\n\
    Base directory will default to \".\" if unspecified.\n\n\
    Options which the driver does not understand are passed onto\n\
    the database as arguments to $sys.startup().\n\n\
Options:\n\n\
    -v               version.\n\
    -b <binary>      binary db directory name, default: \"binary\"\n\
    -r <root>        root file directory name, default: \"root\"\n\
    -t <textdump>    textdump filename to write to, default: \"textdump\"\n\
    -e <bindir>      executables directory name, default: \"root/bin\"\n\
\n",  VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, name);
}

#undef _main_
