#define _defs_

#include "defs.h"

char c_dir_binary[32];
char c_dir_textdump[32];
char c_dir_bin[32];
char c_dir_root[32];
int  c_interactive;
int  running;
long heartbeat_freq;

void init_defs(void) {
    c_interactive = 0;
    running = 1;
    heartbeat_freq = 1;
    strcpy(c_dir_binary,   "binary");
    strcpy(c_dir_textdump, "textdump");
    strcpy(c_dir_bin,      "root/bin");
    strcpy(c_dir_root,     "root");
}

#undef _defs_
