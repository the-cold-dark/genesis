/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef CODE_PRV_H
#define CODE_PRV_H

typedef union instr Instr;
typedef struct handler_positions Handler_positions;

/* An instruction is the code generator's temporary representation of what will
 * become an opcode after the code has been completely compiled and there are
 * no errors. */
union instr {
    Long val;
    char *str;
    Id_list *errors;
};

/* A program is the parser's representation of a program, and is intermediate.
 * The code generator converts a program into a method structure (see
 * method.h), which is the internal form of a ColdC program used by the
 * interpreter. */
struct prog {
    Arguments * args;
    Id_list *   vars;
    Stmt_list * stmts;
};

struct arguments {
    Id_list *ids;
    char *rest;
};

struct stmt {
    Int type;
    Int lineno;
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
    Int type;
    Int lineno;
    union {
	Long num, objnum;
        Float fnum;
	char *name, *symbol, *error, *str;
	Expr *expr;
	Expr_list *args;

	struct {
	    char *name;
	    Expr_list *args;
	} function;

        struct {
            Expr *lval;
            Expr *value;
        } assign;

        struct {
            char *var;
            Expr *value;
        } optassign;

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
	    Expr *handler;
	} frob;

	struct {
	    Expr *list;
	    Expr *offset;
	} index;

	struct {
	    Int opcode;
	    Expr *expr;
	} unary;

	struct {
	    Int opcode;
	    Expr *left;
	    Expr *right;
	} binary;

        struct {
	    Int opcode;
            char *var;
            Expr *value;
        } doeq;

	struct {
	    Expr *left;
	    Expr *right;
	} and, or;

        struct {
	    Expr *src;
	    char *var;
	    Expr *job;
	} map;

        struct {
	    Expr *start;
	    Expr *end;
	    char *var;
	    Expr *job;
	} maprange;

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
    Int lineno;
    Expr_list *values;
    Stmt_list *stmts;
};

struct id_list {
    Int lineno;
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

