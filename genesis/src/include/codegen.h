/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_codegen_h
#define cdc_codegen_h

typedef struct prog		Prog;
typedef struct arguments	Arguments;
typedef struct stmt		Stmt;
typedef struct expr		Expr;
typedef struct case_entry	Case_entry;
typedef struct id_list		Id_list;
typedef struct stmt_list	Stmt_list;
typedef struct expr_list	Expr_list;
typedef struct case_list	Case_list;

extern Pile * compiler_pile;

void init_codegen(void);

Prog * make_prog(Int overridable, Arguments * args, Id_list * vars,
		Stmt_list * stmts);

Arguments * arguments(Id_list * ids, char * rest);

Stmt * comment_stmt(char * comment);
Stmt * noop_stmt(void);
Stmt * expr_stmt(Expr * expr);
Stmt * compound_stmt(Stmt_list * stmt_list);
Stmt * if_stmt(Expr * cond, Stmt * body);
Stmt * if_else_stmt(Stmt * if_, Stmt * false);
Stmt * for_range_stmt(char * id, Expr * lower, Expr * upper, Stmt * body);
Stmt * for_list_stmt(char * id, Expr * list, Stmt * body);
Stmt * while_stmt(Expr * cond, Stmt * body);
Stmt * switch_stmt(Expr * expr, Case_list * cases);
Stmt * break_stmt(void);
Stmt * continue_stmt(void);
Stmt * return_stmt(void);
Stmt * return_expr_stmt(Expr * expr);
Stmt * catch_stmt(Id_list * errors, Stmt * body, Stmt * handler);

Expr * integer_expr(Long num);
Expr * float_expr(Float fnum);
Expr * string_expr(char * str);
Expr * objnum_expr(Long objnum);
Expr * objname_expr(char * name);
Expr * symbol_expr(char * symbol);
Expr * error_expr(char * error);
Expr * var_expr(char * name);
Expr * assign_expr(Expr *lval, Expr * value);
Expr * opt_expr(char *var, Expr *value);
Expr * function_call_expr(char * name, Expr_list * args);
Expr * self_expr_message_expr(Expr * message, Expr_list * args);
Expr * pass_expr(Expr_list * args);
Expr * message_expr(Expr * to, char * message, Expr_list * args);
Expr * expr_message_expr(Expr * to, Expr * message, Expr_list * args);
Expr * list_expr(Expr_list * args);
Expr * dict_expr(Expr_list * args);
Expr * buffer_expr(Expr_list * args);
Expr * frob_expr(Expr * cclass, Expr * rep, Expr *hander);
Expr * index_expr(Expr * list, Expr * offset);
Expr * unary_expr(Int opcode, Expr * expr);
Expr * binary_expr(Int opcode, Expr * left, Expr * right);
Expr * indecr_expr(Int opcode, char * var);
Expr * doeq_expr(Int opcode, char * var, Expr * value);
Expr * and_expr(Expr * left, Expr * right);
Expr * or_expr(Expr * left, Expr * right);
Expr * cond_expr(Expr * cond, Expr * true, Expr * false);
Expr * map_expr(Expr *src, char *var, Expr *job, Int token);
Expr * map_range_expr(Expr *start, Expr *end, char *var, Expr *job, Int token);
Expr * critical_expr(Expr * expr);
Expr * propagate_expr(Expr * expr);
Expr * splice_expr(Expr * expr);
Expr * range_expr(Expr * lower, Expr * upper);

Case_entry * case_entry(Expr_list * values, Stmt_list * stmts);

Id_list * id_list(char * ident, Id_list * next);
Stmt_list * stmt_list(Stmt * stmt, Stmt_list * next);
Expr_list * expr_list(Expr * expr, Expr_list * next);
Case_list * case_list(Case_entry * case_entry, Case_list * next);

Method * generate_method(Prog * prog, Obj * object);

#endif

