/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/modules.h
// ---
//
*/

#ifndef _modules_h_
#define _modules_h_

typedef struct module_s {
    void (*init)(int argc, char ** argv);
    void (*uninit)();
} module_t;

int init_modules(int argc, char ** argv);
int uninit_modules(void);

#endif
