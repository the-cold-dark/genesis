/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/opcodes.h
// ---
// Declarations for the opcodes table.
*/

#ifndef OPCODES_H
#define OPCODES_H

typedef struct op_info Op_info;

#include "ident.h"
#include "object.h"   /* typedef objnum_t */

struct op_info {
    long opcode;
    char *name;
    void (*func)(void);
    int arg1;
    int arg2;
    Ident symbol;
    objnum_t binding;  /* Brandon: 10-Mar-95 */
};

extern Op_info op_table[LAST_TOKEN];

void init_op_table(void);
int find_function(char *name);

#endif

