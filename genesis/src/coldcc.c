/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: coldcc.c
// ---
//
*/

#define _coldcc_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cdc_string.h"               /* strccmp() */
#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "codegen.h"
#include "cdc_types.h"
#include "ident.h"
#include "match.h"
#include "opcodes.h"
#include "util.h"
#include "sig.h"
#include "execute.h"
#include "token.h"
#include "dump.h"
#include "modules.h"
#include "db.h"
#include "cache.h"
#include "object.h"
#include "data.h"
#include "io.h"

#if DISABLED
#include <unistd.h>
#include <time.h>
#endif

#define OPT_COMP 0
#define OPT_DECOMP 1
#define OPT_PARSE 2

int    c_nowrite = 1;
int    c_opt = OPT_COMP;

/* function prototypes */
internal void   initialize(int argc, char **argv);
internal void   usage(char * name);
internal FILE * find_text_db(void);
internal void   compile_db(void);

/*
// --------------------------------------------------------------------
*/
int main(int argc, char **argv) {
    initialize(argc, argv);

    if (c_opt == OPT_DECOMP) {
        fprintf(stderr, "Decompiling database...\n");
        init_binary_db();
        text_dump();
    } else if (c_opt == OPT_COMP) {
        fprintf(stderr, "Compiling database...\n");
        compile_db();
    } else if (c_opt == OPT_PARSE) {
        fprintf(stderr, "This option is not yet supported, sorry\n");
        return 0;
    }

    cache_sync();
    db_close();
    flush_output();

    return 0;
}

/*
// --------------------------------------------------------------------
*/
internal void compile_db() {
    FILE       * fp;
    object_t   * obj;
    list_t     * parents;
    data_t     * d;

    init_new_db();
    fp = find_text_db();

    /* create the root object */
    obj = cache_retrieve(ROOT_DBREF);
    if (!obj) {
        parents = list_new(0);
        obj = object_new(ROOT_DBREF, parents);
        list_discard(parents);
    }
    cache_discard(obj);

    /* create the system object */
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

    fp = fopen(c_dir_textdump, "r");
    if (!fp) {
        fprintf(stderr, "Couldn't open text dump file \"%s\".\n",
                c_dir_textdump);
        exit(1);
    } else {
        text_dump_read(fp);
        fclose(fp);
    }

    fprintf(stderr, "Database compiled from \"%s\" to \"%s\".\n",
            c_dir_textdump, c_dir_binary);
}

/*
// --------------------------------------------------------------------
// Finds target the database is, based off input name.
//
// If name is:
//
//    "stdin", will read from stdin.
//    a directory, will do directory compilation (unsupported)
//    a valid file, will use that (textdump)
//
// Will output to global name "output", set by options
*/

internal FILE * find_text_db(void) {
    FILE        * fp = NULL;

    if (strccmp(c_dir_textdump, "stdin") == F_SUCCESS) {
        fputs("Reading from stdin.\n", stderr);
        fp = stdin;
    } else {
        struct stat sbuf;

        if (stat(c_dir_textdump, &sbuf) == F_FAILURE) {
            fprintf(stderr, "** Unable to open target \"%s\".\n",
                    c_dir_textdump);
            exit(1);
        }

        if (S_ISDIR(sbuf.st_mode)) {
            fputs("** Directory based compilation is currently unsupported.\n",
                  stderr);
            fclose(fp);
            exit(0);
        }

        /* just let fopen deal with the other perms */
        fp = fopen(c_dir_textdump, "r");
        if (fp == NULL) {
            fprintf(stderr, "** bad happened.\n");
            exit(1);
        }

        fprintf(stderr, "Reading from \"%s\".\n", c_dir_textdump);
    }

    return fp;
}

/*
// --------------------------------------------------------------------
// Initializes tables, variables and other aspects for use
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

internal void initialize(int argc, char **argv) {
    char * name = NULL,
         * opt = NULL;

    name = *argv;

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
    init_cache();
    init_defs();

    argv++;
    argc--;

    while (argc) {
        if (**argv == '-') {
            opt = *argv;
            opt++;
            switch (*opt) {
                case 'v':
                    printf("ColdCC %d.%d-%d\n",
                           VERSION_MAJOR,
                           VERSION_MINOR,
                           VERSION_PATCH);
                    exit(0);
                case 'c':
                    c_opt = OPT_COMP;
                    break;
                case 'd':
                    c_opt = OPT_DECOMP;
                    break;
                case 'p':
                    c_opt = OPT_PARSE;
                    fputs("\n** Unsupported option: -p\n", stderr);
                    break;
                case 'w':
                    fputs("\n** Unsupported option: -w\n", stderr);
                    c_nowrite = 0;
                    break;
                case 't':
                    nextarg();
                    strcpy(c_dir_textdump, *argv);
                    break;
                case 'o':
                case 'b':
                    nextarg();
                    strcpy(c_dir_binary, *argv);
                    break;
                case '-':
                case 'h':
                    usage(name);
                    exit(0);
                default:
                    usage(name);
                    fprintf(stderr, "\n** Invalid Option: -%s\n", opt);
                    exit(1);
            }
        }
        argv++;
        argc--;
    }
}

/*
// --------------------------------------------------------------------
// Simple usage message, rather explanatory
*/
internal void usage (char * name) {
    fprintf(stderr, "\n-- ColdCC %d.%d-%d --\n\n\
Usage: %s [options]\n\
\n\
Options:\n\n\
        -v              version\n\
        -h              This message.\n\
        -d              Decompile.\n\
        -c              Compile (default).\n\
        -b <binary>     binary db directory name, default: \"binary\"\n\
        -t <target>     target text db, default: \"textdump\"\n\
                        if this is \"stdin\" it will read from stdin\n\
                        instead.  <target> may be a directory or file.\n\n\
Anticipated Options:\n\n\
        -p              Partial compile, compile object(s) and insert\n\
                        into database accordingly.  Can be used with -w\n\
                        for a ColdC verifier.\n\
        -w              Compile for parse only, do not write output.\n\
                        This option can only be used with partial compile.\n\
\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, name);
}

#undef _coldcc_
