/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_native_h
#define cdc_native_h

#include <stdio.h>
#include "file.h"
#include "execute.h"

/* this structure is used only to initialize methods */
/* we pull the name symbol and put it in the actual method definition */
/* we need num_args defined here so we can drop it into the method def */
typedef struct native_s {
    char     * bindobj;
    char     * name;
    Int       (*func)(Int stack_start, Int arg_start);
} native_t;

 /* ANSI doesn't want us to us NULL pointers to functions */
typedef struct module_s {
    Bool   init;
    void (*init_func)(Int argc, char ** argv);
    Bool   uninit;
    void (*uninit_func)(void);
} module_t;

Int init_modules(Int argc, char ** argv);
Int uninit_modules(void);
Int add_native_methods(void);

#include "macros.h"

#endif
