/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_opcodes_h
#define cdc_opcodes_h

typedef struct op_info Op_info;

struct op_info {
    Long opcode;
    char *name;
    void (*func)(void);
    Int arg1;
    Int arg2;
    Ident symbol;
    cObjnum binding;
};

extern Op_info op_table[LAST_TOKEN];

void init_op_table(void);
Int find_function(char *name);

#endif

