/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/code_prv.h
// ---
// Representation of code generation structures.
*/

#ifndef CODE_PRV_H
#define CODE_PRV_H

typedef union instr Instr;
typedef struct handler_positions Handler_positions;

#include "codegen.h"

/* An instruction is the code generator's temporary representation of what will
 * become an opcode after the code has been completely compiled and there are
 * no errors. */
union instr {
    long val;
    char *str;
    Id_list *errors;
};

/* A program is the parser's representation of a program, and is intermediate.
 * The code generator converts a program into a method structure (see
 * method.h), which is the internal form of a ColdC program used by the
 * interpreter. */
struct prog {
    int m_flags;
    int m_access;
    Arguments * args;
    Id_list *   vars;
    Stmt_list * stmts;
};

struct arguments {
    Id_list *ids;
    char *rest;
};

struct stmt {
    int type;
    int lineno;
    union {
	char *comment;
	Expr *expr;
	Stmt_list *stmt_list;

	struct {
	    Expr *cond;
	    Stmt *true;
	    Stmt *false;
	} if_;

	struct {
	    char *var;
	    Expr *lower;
	    Expr *upper;
	    Stmt *body;
	} for_range;

	struct {
	    char *var;
	    Expr *list;
	    Stmt *body;
	} for_list;

	struct {
	    Expr *cond;
	    Stmt *body;
	} while_;

	struct {
	    Expr *expr;
	    Case_list *cases;
	} switch_;

	struct {
	    Id_list *errors;
	    Stmt *body;
	    Stmt *handler;
	} ccatch;

	struct {
	    Expr *time;
	    Stmt *body;
	} fork;

    } u;
};

struct expr {
    int type;
    int lineno;
    union {
	long num, objnum;
        float fnum;
	char *name, *symbol, *error, *str;
	Expr *expr;
	Expr_list *args;

	struct {
	    char *name;
	    Expr_list *args;
	} function;

        struct {
            char *var;
            Expr *value;
        } assign;

	struct {
	    Expr *message;
	    Expr_list *args;
	} self_expr_message;

	struct {
	    Expr *to;
	    char *name;
	    Expr_list *args;
	} message;

	struct {
	    Expr *to;
	    Expr *message;
	    Expr_list *args;
	} expr_message;

	struct {
	    Expr *cclass;
	    Expr *rep;
	} frob;

	struct {
	    Expr *list;
	    Expr *offset;
	} index;

	struct {
	    int opcode;
	    Expr *expr;
	} unary;

	struct {
	    int opcode;
	    Expr *left;
	    Expr *right;
	} binary;

	struct {
	    Expr *left;
	    Expr *right;
	} and, or;

	struct {
	    Expr *cond;
	    Expr *true;
	    Expr *false;
	} cond;

	struct {
	    Expr *lower;
	    Expr *upper;
	} range;

    } u;
};

struct case_entry {
    int lineno;
    Expr_list *values;
    Stmt_list *stmts;
};

struct id_list {
    int lineno;
    char *ident;
    Id_list *next;
};

struct stmt_list {
    Stmt *stmt;
    Stmt_list *next;
};

struct expr_list {
    Expr *expr;
    Expr_list *next;
};

struct case_list {
    Case_entry *case_entry;
    Case_list *next;
};

#endif

