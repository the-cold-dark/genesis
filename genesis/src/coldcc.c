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

#include "config.h"
#include "defs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include "cdc_string.h"               /* strccmp() */
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
#include "modules.h"
#include "binarydb.h"
#include "textdb.h"
#include "cache.h"
#include "object.h"
#include "data.h"
#include "io.h"
#include "file.h"

#if DISABLED
#include <unistd.h>
#include <time.h>
#endif

#define OPT_COMP 0
#define OPT_DECOMP 1
#define OPT_PARTIAL 2

int    c_nowrite = 1;
int    c_opt = OPT_COMP;

#define NEW_DB       1
#define EXISTING_DB  0

/* function prototypes */
INTERNAL void   initialize(int argc, char **argv);
INTERNAL void   usage(char * name);
INTERNAL FILE * find_text_db(void);
INTERNAL void   compile_db(int type);

/*
// --------------------------------------------------------------------
*/
int main(int argc, char **argv) {
    initialize(argc, argv);

    if (c_opt == OPT_DECOMP) {
        fprintf(stderr, "Decompiling database...\n");
        init_binary_db();
        init_core_objects();
        text_dump();
    } else if (c_opt == OPT_COMP) {
        fprintf(stderr, "Compiling database...\n");
        compile_db(NEW_DB);
    } else if (c_opt == OPT_PARTIAL) {
        fprintf(stderr, "Opening database for partial compile...\n");
        compile_db(EXISTING_DB);
    }

    cache_sync();
    db_close();
    flush_output();
    close_files();

    fputc(10, logfile);

    return 0;
}

/*
// --------------------------------------------------------------------
*/
INTERNAL void compile_db(int newdb) {
    FILE       * fp;

    /* create a new db, this will blast old dbs */
    if (newdb)
        init_new_db();
    else
        init_binary_db();

    /* get new db */
    fp = find_text_db();

    /* verify $root/#1 and $sys/#0 exist */
    init_core_objects();

    if (!fp) {
        fprintf(stderr, "Couldn't open text dump file \"%s\".\n",
                c_dir_textdump);
        exit(1);
    } else {
        /* text_dump_read(fp); */
        compile_cdc_file(fp);
        fclose(fp);
    }

    fprintf(stderr, "Database compiled to \"%s\"\nClosing binary database...",
            c_dir_binary);
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

INTERNAL FILE * find_text_db(void) {
    FILE        * fp = NULL;

    if (strccmp(c_dir_textdump, "stdin") == 0) {
        fputs("Reading from STDIN.\n", stderr);
        return stdin;
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
        fp = fopen(c_dir_textdump, "rb");
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

#define NEWFILE(var, name) { \
        free(var); \
        var = EMALLOC(char, strlen(name)); \
        strcpy(var, name); \
    }

INTERNAL void initialize(int argc, char **argv) {
    char * name = NULL,
         * opt = NULL,
         * buf;

    name = *argv;

    init_defs();

    init_codegen();
    init_ident();
    init_op_table();
    init_match();
    init_util();
    init_execute();
    init_scratch_file();
    init_token();
    init_modules(argc, argv);
    init_cache();

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
                    c_opt = OPT_PARTIAL;
                    break;
                case 'w':
                    fputs("\n** Unsupported option: -w\n", stderr);
                    c_nowrite = 0;
                    break;
                case 't':
                    argv += getarg(name,&buf, opt, argv,&argc,usage);
                    NEWFILE(c_dir_textdump, buf);
                    break;
                case 'o':
                case 'b':
                    argv += getarg(name,&buf, opt, argv, &argc, usage);
                    NEWFILE(c_dir_binary, buf);
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
void usage (char * name) {
    fprintf(stderr, "\n-- ColdCC %d.%d-%d --\n\n\
Usage: %s [options]\n\
\n\
Options:\n\n\
        -v              version\n\
        -h              This message.\n\
        -d              Decompile.\n\
        -c              Compile (default).\n\
        -b binary       binary db directory name, default: \"binary\"\n\
        -t target       target text db, default: \"textdump\"\n\
                        if this is \"stdin\" it will read from stdin\n\
                        instead.  <target> may be a directory or file.\n\
        -p              Partial compile, compile object(s) and insert\n\
                        into database accordingly.  Can be used with -w\n\
                        for a ColdC code verification program.\n\
Anticipated Options:\n\n\
        -w              Compile for parse only, do not write output.\n\
                        This option can only be used with partial compile.\n\
\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, name);
}

#undef _coldcc_

