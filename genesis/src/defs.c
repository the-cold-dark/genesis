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

#define INIT_VAR(var, name, len) { \
        var = EMALLOC(char, len + 1); \
        strncpy(var, name, len); \
        var[len] = (char) NULL; \
    }

void init_defs(void) {
    c_interactive = 0;
    running = 1;
    atomic = 0;
    heartbeat_freq = 5;

    INIT_VAR(c_dir_binary, "binary", 6);
    INIT_VAR(c_dir_textdump, "textdump", 8);
    INIT_VAR(c_dir_bin, "dbbin", 8);
    INIT_VAR(c_dir_root, "root", 4);
    INIT_VAR(c_logfile, "logs/db.log", 11);
    INIT_VAR(c_errfile, "logs/driver.log", 15);
    INIT_VAR(c_pidfile, "logs/genesis.pid", 16);

    logfile = stdout;
    errfile = stderr;
}

#undef INIT_VAR

#undef _defs_
