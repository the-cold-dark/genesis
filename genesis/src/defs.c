#define _defs_

#include "defs.h"
#include "memory.h"

/*char c_dir_binary[32];
#char c_dir_textdump[32];
#char c_dir_bin[32];
#char c_dir_root[32];
int  c_interactive;
int  running;
int  atomic;
long heartbeat_freq;
*/

#define INIT_VAR(var, name) { \
        var = EMALLOC(char, strlen(name)); \
        strcpy(var, name); \
    }

void init_defs(void) {
    c_interactive = 0;
    running = 1;
    atomic = 0;
    heartbeat_freq = 1;

    INIT_VAR(c_dir_binary, "binary");
    INIT_VAR(c_dir_textdump, "textdump");
    INIT_VAR(c_dir_bin, "root/bin");
    INIT_VAR(c_dir_root, "root");
    INIT_VAR(c_logfile, "logs/server.log");
    INIT_VAR(c_errfile, "logs/error.log");
    INIT_VAR(c_pidfile, "logs/genesis.pid");

    logfile = stdout;
    errfile = stderr;
}

#undef INIT_VAR

#undef _defs_
