/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define _coldcc_

#include "defs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>

#ifdef USE_CLEANER_THREAD
extern pthread_t cleaner;
#endif

#include "cdc_pcode.h"
#include "cdc_db.h"
#include "coldcc.h"
#include "textdb.h"

#include "strutil.h"
#include "util.h"
#include "sig.h"
#include "moddef.h"

#define OPT_COMP 0
#define OPT_DECOMP 1
#define OPT_PARTIAL 2

Int    c_nowrite = 1;
Int    c_opt = OPT_COMP;
Bool   print_objs = YES;
Bool   print_names = NO;
Bool   print_invalid = YES;
Bool   print_warn = YES;

#define NEW_DB       1
#define EXISTING_DB  0

/* function prototypes */
static void   initialize(Int argc, char **argv);
static void   usage(char * name);
static FILE * find_text_db(void);
static void   compile_db(Int type);

void shutdown_coldcc(int exit_status) {
    running = NO;
    write_err("Syncing binarydb...");
    cache_sync();
    simble_close();
    object_extra_cleanup_all();
    uninit_cache();
    uninit_scratch_file();
    uninit_execute();
    uninit_op_table();
    uninit_ident();
    uninit_codegen();
    uninit_match();
    uninit_defs();

    /* do this manually, genesis does it from an atexit routine */
    efree(c_runfile);

    uninit_emalloc();
    write_err("Done");
    exit(exit_status);
}

/*
// --------------------------------------------------------------------
*/

/*
// --------------------------------------------------------------------
*/
int main(int argc, char **argv) {
    initialize(argc, argv);

    if (setjmp(main_jmp) == 0) {
        if (c_opt == OPT_DECOMP) {
            init_binary_db();
            init_core_objects();
            write_err ("Writing to \"%s\"..", c_dir_textdump);
            text_dump(print_names);
        } else if (c_opt == OPT_COMP) {
            write_err ("Compiling database...");
            compile_db(NEW_DB);
        } else if (c_opt == OPT_PARTIAL) {
            write_err ("Opening database for partial compile...");
            compile_db(EXISTING_DB);
        }
    }

    write_err ("Closing binary database...");
    shutdown_coldcc(EXIT_SUCCESS);

    /* make compilers happy; we never reach this */
    return 0;
}

/*
// --------------------------------------------------------------------
*/
static void compile_db(Int newdb) {
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
        write_err ("Couldn't open text dump file \"%s\".",
                c_dir_textdump);
        exit(1);
    } else {
        /* text_dump_read(fp); */
        compile_cdc_file(fp);
        fclose(fp);
    }

    write_err ("Database compiled to \"%s\"", c_dir_binary);
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

static FILE * find_text_db(void) {
    FILE        * fp = NULL;

    if (strccmp(c_dir_textdump, "stdin") == 0) {
        write_err("Reading from STDIN...");
        return stdin;
    } else {
        struct stat sbuf;

        if (stat(c_dir_textdump, &sbuf) == F_FAILURE) {
            write_err("** Unable to open target \"%s\".", c_dir_textdump);
            exit(1);
        }

        if (S_ISDIR(sbuf.st_mode)) {
            write_err("** Directory based compilation is currently unsupported.");
            fclose(fp);
            exit(0);
        }

        /* just let fopen deal with the other perms */
        fp = fopen(c_dir_textdump, "rb");
        if (fp == NULL) {
            write_err("** bad happened.");
            exit(1);
        }

        write_err("Reading from \"%s\".", c_dir_textdump);
    }

    return fp;
}

/*
// --------------------------------------------------------------------
// print all of the natives nicely.
//
// be cute and columnize
*/

static void print_natives(void) {
    Int          mid, x;
    char         buf[LINE];

    if (NATIVE_LAST % 2)
        mid = (NATIVE_LAST + 1) / 2;
    else
        mid = NATIVE_LAST / 2;

    fputs("Native Method Configuration:\n\n", stdout);

    for (x=0; mid < NATIVE_LAST; x++, mid++) {
        sprintf(buf, "$%s.%s()", natives[x].bindobj, natives[x].name);
        printf("  %-37s $%s.%s()\n", buf, natives[mid].bindobj,
               natives[mid].name);
    }

    if (NATIVE_LAST % 2) {
        x++;
        printf("  $%s.%s()\n", natives[x].bindobj, natives[x].name);
    }

    fputc(10, stdout);
    fflush(stdout);
}

/*
// --------------------------------------------------------------------
// Initializes tables, variables and other aspects for use
*/

#define NEWFILE(var, name) { \
        free(var); \
        var = EMALLOC(char, strlen(name) + 1); \
        strcpy(var, name); \
    }

static void initialize(Int argc, char **argv) {
    Bool   opt_bool = NO;
    char * name = NULL,
         * opt = NULL,
         * buf;

    name = *argv;

    use_natives = 0;

    argv++;
    argc--;

    init_defs();
    init_match();
    init_util();

    while (argc) {
        if (**argv == '-' || **argv == '+') {
            opt = *argv;
            opt_bool = (*opt == '+');
            opt++;
            switch (*opt) {
                case 'v':
                    printf("ColdCC %d.%d-%d\n",
                           VERSION_MAJOR,
                           VERSION_MINOR,
                           VERSION_PATCH);
                    exit(0);
                case 'f':
                    use_natives = FORCE_NATIVES;
                    break;
                case 'n':
                    print_natives();
                    exit(0);
                    break;
                case 'c':
                    c_opt = OPT_COMP;
                    break;
                case 'd':
                    c_opt = OPT_DECOMP;
                    break;
                case '#':
                    print_names = !opt_bool;
                    break;
                case 'o':
                    print_objs = opt_bool;
                    break;
                case 'p':
                    c_opt = OPT_PARTIAL;
                    break;
                case 's': {
                    char * p;

                    argv += getarg(name, &buf, opt, argv, &argc, usage);
                    p = buf;
                    cache_width = atoi(p);
                    while (*p && isdigit(*p))
                        p++;
                    if ((char) LCASE(*p) == 'x') {
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
                case 'W':
                    print_warn = NO;
                    break;
                case 'w':
                    write_err("\n** Unsupported option: -w");
                    c_nowrite = 0;
                    break;
                case 't':
                    argv += getarg(name,&buf, opt, argv,&argc,usage);
                    NEWFILE(c_dir_textdump, buf);
                    break;
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
                    write_err("\n** Invalid Option: -%s", opt);
                    exit(1);
            }
        }
        argv++;
        argc--;
    }

    init_sig();
#ifdef DRIVER_DEBUG
    init_debug();
#endif
    init_codegen();
    init_ident();
    init_op_table();
    init_execute();
    init_scratch_file();
    init_token();
    init_modules(argc, argv);
    init_instances();
    init_cache(FALSE);

    /* force coldcc to be atomic, specify that we are not running online */
    atomic = YES;
    coldcc = YES;
}

/*
// --------------------------------------------------------------------
// Simple usage message, rather explanatory
*/
void usage (char * name) {
    fprintf (stderr,
             "\n-- ColdCC %d.%d-%d --\n\n"
             "Usage: %s [options]\n"
             "\n"
             "Options:\n\n"
             "    -f              force native methods to override existing methods.\n"
             "    -v              version\n"
             "    -h              This message.\n"
             "    -d              Decompile.\n"
             "    -c              Compile (default).\n"
             "    -b binary       binary db directory name, current: \"%s\"\n"
             "    -t target       target text db, current: \"%s\"\n"
             "                    If this is \"stdin\" it will read from stdin\n"
             "                    instead.  <target> may be a directory or file.\n"
             "    -p              Partial compile, compile object(s) and insert\n"
             "                    into database accordingly.  Can be used with -w\n"
             "                    for a ColdC code verification program.\n"
             "    +|-#            Print/Do not print object numbers by default.\n"
             "                    Default option is +#\n"
             "                    print object names by default, if they exist.\n"
             "    -s WIDTHxDEPTH  Cache size, default %dx%d\n"
             "    -n              List native method configuration.\n"
             "    +|-o            Print/Do not print objects as they are processed.\n"
             "    -W              Do not print warnings.\n"
             "\n\n",
             VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, name, c_dir_binary, c_dir_textdump,
             CACHE_WIDTH, CACHE_DEPTH);
    fflush(stderr);
}

#undef _coldcc_

