/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Main file for 'genesis' executable
*/

#define _main_

#include "defs.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef __UNIX__
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <grp.h>
#include <pwd.h>
#endif
#include <ctype.h>
#include <time.h>
#include "cdc_pcode.h"
#include "cdc_db.h"
#include "strutil.h"
#include "util.h"
#include "file.h"
#include "net.h"
#include "sig.h"

#ifdef __MSVC__
#include <direct.h>
#include <process.h>
#endif

INTERNAL void initialize(Int argc, char **argv);
INTERNAL void main_loop(void);

#ifdef __UNIX__
uid_t uid;
gid_t gid;
#endif

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
*/

INTERNAL void prebind_port_with(char * str, char * name) {
    int port;
    char * addr = NULL, * s = str;
    Bool tcp = YES;

    if (isdigit(*s)) {
        addr = s;
        while (*s && *s != ':') s++;
        if (!*s) {
            usage(name);
            fprintf(stderr, "** No port given with -p %s\n", str);
            exit(1);
        }
    }

    if (*s != ':') {
        usage(name);
        fprintf(stderr, "** Invalid prebind format: %s\n", str);
        exit(1);
    }
    *s = (char) NULL;
    s++;

    port = atoi(s);

    if (port < 0) {
        tcp = NO;
        port = -port;
    } else if (port == 0) {
        usage(name);
        fprintf(stderr, "** Invalid port: 0\n");
        exit(1);
    }

    /* now prebind it */
    if (prebind_port(port, addr, tcp)) {
        if (addr)
            fprintf(stderr, "prebound %s:%d\n", addr, tcp ? port : -port);
        else
            fprintf(stderr, "prebound :%d\n", tcp ? port : -port);
    } else {
        fprintf(stderr, "** unable to prebind port: Invalid address\n");
        exit(1);
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

    /* Flush defunct sockets, sync the cache,
     * flush output buffers, and exit normally.
     */
    flush_defunct();
    cache_sync();
    db_close();
    flush_output();
    close_files();
    uninit_scratch_file();
    if (errfile)
        fclose(errfile);
    if (logfile)
        fclose(logfile);

    return 0;
}

/*
// --------------------------------------------------------------------
//
// Initialization
//
*/
void get_my_hostname(void) {
    char   cbuf[LINE];

    /* for those OS's that do not do this */
    memset(cbuf, 0, LINE);
    if (!gethostname(cbuf, LINE)) {
        if (cbuf[LINE-1] != (char) NULL) { 
            fprintf(stderr, "Unable to determine hostname: name too long.\n");
        } else {
            string_discard(str_hostname);
            str_hostname = string_from_chars(cbuf, strlen(cbuf));
        }
    } else {
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

/*
// --------------------------------------------------------------------
*/

INTERNAL void initialize(Int argc, char **argv) {
    cList   * args;
    cStr     * str;
    cData     arg;
    char     * opt = NULL,
             * name = NULL,
             * basedir = NULL,
             * buf = NULL;
    FILE     * fp;
    Bool       dofork = YES;
    pid_t      pid;

    name = *argv;
    argv++;
    argc--;

    /* Ditch stdin, so we can reuse the file descriptor */
    fclose(stdin);

    /* basic initialization */
#ifdef __UNIX__
    uid = getuid();
    gid = getgid();
#endif
    init_defs();
    init_match();
    init_util();
    init_ident();

    /* db argument list */
    args = list_new(0);

    /* parse arguments */
    while (argc) {
        opt = *argv;
        if (*opt == '-') {
            opt++;
 
            /* catch db options */
            if (*opt == '-') {
                addarg(opt);
                goto end;
            }

            switch (*opt) {
            case 'd': /* directory */
                opt++;
                if (*opt == (char) NULL) {
                    usage(name);
                    fputs("** Invalid directory option: -d\n", stderr);
                    exit(1);
                }
                argv += getarg(name,&buf,opt,argv,&argc,usage);
                switch (*opt) { 
                  case 'b':
                      NEWFILE(c_dir_binary, buf);
                      break;
                  case 'r':
                      NEWFILE(c_dir_root, buf);
                      break;
                  case 'x':
                      NEWFILE(c_dir_bin, buf);
                      break;
                  default:
                    usage(name);
                    fprintf(stderr, "** Invalid directory option: -d%c\n",*opt);
                    exit(1);
                }
                break;
            case 'l': /* logfile */
                opt++;
                if (*opt == (char) NULL) {
                    usage(name);
                    fputs("** Invalid file option: -l\n", stderr);
                    exit(1);
                }
                argv += getarg(name,&buf,opt,argv,&argc,usage);
                switch (*opt) { 
                  case 'd':
                      NEWFILE(c_logfile, buf);
                      break;
                  case 'g':
                      NEWFILE(c_errfile, buf);
                      break;
                  case 'p':
                      NEWFILE(c_runfile, buf);
                      break;
                  default:
                      usage(name);
                      fprintf(stderr, "** Invalid file option: -l%c\n",*opt);
                      exit(1);
                }
                break;
            case 'f':
                dofork = NO;
                break;
            case 'p':
                argv += getarg(name,&buf,opt,argv,&argc,usage);
                prebind_port_with(buf, name);
                break;
            case 'h':
                usage(name);
                exit(0);
                break;
            case 'v':
                printf("Genesis %d.%d-%d\n",
                       VERSION_MAJOR,
                       VERSION_MINOR,
                       VERSION_PATCH);
                exit(0);
                break;
            case 'n':
                argv += getarg(name,&buf,opt,argv,&argc,usage);
                string_discard(str_hostname);
                str_hostname = string_from_chars(buf, strlen(buf));
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
#ifdef __UNIX__
            case 'g': {
                struct group * gr; 
 
                argv += getarg(name,&buf,opt,argv,&argc,usage);
                if (buf[0] == '#') {
                    gid = (gid_t) atoi(&buf[1]);
                } else if (!(gr = getgrnam(buf))) {
                    usage(name);
                    fprintf(stderr,
                            "** invalid group name -g: '%s'\n",buf);
                    exit(1);
                } else {
                    gid = (gid_t) gr->gr_gid;
                }
                break;
              }
            case 'u': {
                struct passwd * pw; 
 
                argv += getarg(name,&buf,opt,argv,&argc,usage);
                if (buf[0] == '#') {
                    uid = (uid_t) atoi(&buf[1]);
                } else if (!(pw = getpwnam(buf))) {
                    usage(name);
                    fprintf(stderr,
                            "** invalid user name -u: '%s'\n",buf);
                    exit(1);
                } else {
                    uid = (uid_t) pw->pw_uid;
                }
                break;
              }
#endif
            default:
                usage(name);
                fprintf(stderr, "** Invalid argument -%s\n** send arguments to the database by prefixing them with '--', not '-'\n", opt); 
                exit(1);
            }
        } else {
            if (basedir == NULL)
                basedir = *argv;
            else
                addarg(*argv);
        }

        end:
        argv++;
        argc--;
    }

    /* Initialize internal tables and variables. */
#ifdef DRIVER_DEBUG
    init_debug();
#endif
    init_codegen();
    init_op_table();
    init_sig();
    init_execute();
    init_token();
    init_modules(argc, argv);
    init_net();
    init_instances();

    /* Figure out our hostname */
    if (!string_length(str_hostname))
        get_my_hostname();

    /* where is the base db directory ? */
    if (basedir == NULL)
        basedir = ".";

    /* Switch into database directory. */
#ifdef __MSVC__
    if (_chdir(basedir) == F_FAILURE)
#else
    if (chdir(basedir) == F_FAILURE)
#endif
    {
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

    /*
    // Clean up our execution privs
    */
#ifdef __UNIX__

#define ROOT_UID 0
#define DIE(_msg_) { fprintf(errfile, _msg_); fputc('\n', errfile); exit(1); }

    if (gid != getgid()) {
        if (geteuid() != ROOT_UID)
            DIE("** setgid attempted when not running as root, exiting..")
        if (setgid(gid) == F_FAILURE)
            DIE("** setgid(): unable to change group, exiting..")
    }

    if (uid != getuid()) {
        if (geteuid() != ROOT_UID)
            DIE("** setuid attempted when not running as root, exiting..")
        if (setuid(uid) == F_FAILURE)
            DIE("** setuid(): unable to change user, exiting..")
    }

#undef ROOT_UID
#undef DIE

#endif

    /*
    // Now fork a child, this will also have the effect of clearing
    // any residual benefits of setuid()
    */
#ifdef __UNIX__
    if (dofork) {
        pid = FORK_PROCESS();
        if (pid != 0) {
            int ignore;
            if (pid == -1)
                fprintf(stderr,"genesis: unable to fork: %s\n",
                                strerror(GETERR()));
            else if (waitpid(pid, &ignore, WNOHANG) == F_FAILURE)
                fprintf(stderr,"genesis: waitpid: %s\n",
                                strerror(GETERR()));
            exit(0);
        }
    }
#endif

    /* print the PID */
    if ((fp = fopen(c_runfile, "wb")) != NULL) {
#ifdef __MSVC__
        fprintf(fp, "%ld\n", (long) _getpid());
#else
        fprintf(fp, "%ld\n", (long) getpid());
#endif
        fclose(fp);
        atexit(unlink_runningfile);
    } else {
#ifdef __MSVC__
        fprintf(errfile, "genesis pid: %ld\n", (long) _getpid());
#else
        fprintf(errfile, "genesis pid: %ld\n", (long) getpid());
#endif
    }

    /* Initialize database and network modules. */
    init_scratch_file();
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
            str = data_to_literal(d, DF_WITH_OBJNAMES);
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
    vm_task(SYSTEM_OBJNUM, startup_id, 1, &arg);
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
        } else {
            /*
            // If no heartbeat is set, we should still resume
            // periodically, even if no I/O events have occurred.
            */
            seconds = preempted ? 0 : NO_HEARTBEAT_INTERVAL;
        }

        /* push our dump along, diddle with the wait if we need to */
        switch (dump_some_blocks(DUMP_BLOCK_SIZE)) {
            case DUMP_FINISHED:
                finish_backup();
                vm_task(SYSTEM_OBJNUM, backup_done_id, 0);
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
                vm_task(SYSTEM_OBJNUM, heartbeat_id, 0);
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

/*
// --------------------------------------------------------------------
*/

void usage (char * name) {
    fprintf(stderr, "\n-- Genesis %d.%d-%d --\n\n\
Usage: %s [base dir] [options]\n\n\
    Base directory will default to \".\" if unspecified.  Arguments which\n\
    the driver does not recognize, or options which begin with \"--\" rather\n\
    than \"-\" are passed onto the database as arguments to $sys.startup().\n\n\
    Note: specifying \"stdin\" or \"stderr\" for either of the logs will\n\
    direct them appropriately.\n\n\
Options:\n\n\
    -v          version.\n\
    -f          do not fork on startup\n\
    -db <dir>   alternate binary directory, current: \"%s\"\n\
    -dr <dir>   alternate root file directory, current: \"%s\"\n\
    -dx <dir>   alternate executables directory, current: \"%s\"\n\
    -ld <file>  alternate database logfile, current: \"%s\"\n\
    -lg <file>  alternate driver (genesis) logfile, current: \"%s\"\n\
    -lp <file>  alternate runtime pid logfile, current: \"%s\"\n\
    -s <size>   Cache size, given as WIDTHxDEPTH, current: %dx%d\n\
    -n <name>   specify the hostname (rather than looking it up)\n\
    -u <user>   if running as root, setuid to this user.  This only works\n\
                in unix.  Genesis must first be run as root.\n\
    -g <group>  if running as root, setgid to this group.  This only works\n\
                in unix.  Genesis must first be run as root.\n\
    -p <port>   prebind port, can exist multiple times.  Port must be\n\
                formatted as: [ADDR]:PORT. UDP ports are specified with\n\
                negative numbers. Address is any, if unspecified, or must\n\
                be an IP address.  All below are valid:\n\n\
                    206.81.134.103:80\n\
                    :-20\n\
                    :23\n\n",

     VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, name, c_dir_binary,
     c_dir_root, c_dir_bin, c_logfile, c_errfile, c_runfile, cache_width,
     cache_depth);
}

/* TEMPORARY-- we need an area where identical functions 'names' (yet
   different behaviours) between genesis/coldcc exist */

cObjnum get_object_name(Ident id) {
    cObjnum num;

    if (!lookup_retrieve_name(id, &num))
        num = db_top++;
    return num;
}
