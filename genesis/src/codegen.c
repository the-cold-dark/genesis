/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Generate internal representation for ColdC code.
*/

#include "defs.h"

#include <string.h>

#include "cdc_pcode.h"
#include "util.h"

/* We use MALLOC_DELTA to keep instr_buf thirty-two bytes less than a power of
 * two, assuming an Instr and Int is four bytes. */
/* HACKNOTE: BAD */
#define MALLOC_DELTA		8
#define INSTR_BUF_START 	(512 - MALLOC_DELTA)
#define JUMP_TABLE_START	(128 - MALLOC_DELTA)
#define MAX_VARS		128

enum scatter_modes {
    SM_STANDARD,
    SM_OPTIONAL,
    SM_REST,
    SM_ERROR
};

static void compile_stmt_list(Stmt_list *stmt_list, Int loop, Int catch_level);
static void compile_stmt(Stmt *stmt, Int loop, Int catch_level);
static void compile_cases(Case_list *cases, Int loop, Int catch_level,
			  Int end_dest);
static void compile_case_values(Expr_list *values, Int body_dest);
static void compile_expr_list(Expr_list *expr_list);
static void compile_variable_assign (char *var);
static void compile_optional_assign (char *var, Expr *e);
static enum scatter_modes compile_assign (Expr *e, enum scatter_modes mode,
					  Bool in_scatter);
static void compile_expr(Expr *expr);
static Int find_local_var(char *id);
static void check_instr_buf(Int pos);
static void code(Long val);
static void code_str(char *str);
static void code_errors(Id_list *errors);
static Int new_jump_dest(void);
static void set_jump_dest_here(Int dest);
static Int id_list_size(Id_list *id_list);
static Method *final_pass(Obj *object);

/* Temporary instruction storage. */
static Instr *instr_buf;
static Int instr_loc, instr_size;

/* The jump destination table. */
static Int *jump_table, jump_loc, jump_size;

/* For convenience.  Set by generate_method(). */
static Prog *the_prog;

/* Keep track of the number of error lists we'll need. */
static Int num_error_lists;

Pile *compiler_pile;			/* Temporary storage pile. */

/* Requires: Shouldn't be called twice.
 * Modifies: compiler_pile, instr_buf, instr_size.
 * Effects: Initializes the compiler pile and instruction buffer. */
void init_codegen(void)
{
    /* Initialize the compiler pile.  We allocate memory from this pile when
     * we compile, and then free it all at once when we're done. */
    compiler_pile = new_pile();

    /* Initialize instruction buffer. */
    instr_buf = EMALLOC(Instr, INSTR_BUF_START);
    instr_size = INSTR_BUF_START;

    /* Initialize jump table. */
    jump_table = EMALLOC(Int, JUMP_TABLE_START);
    jump_size = JUMP_TABLE_START;
}

/* All constructors allocate memory from compiler_pile, so their results are
 * only good until the next pfree() of compiler_pile.  Their effects should
 * all be self-explanatory unless otherwise noted. */
Prog *make_prog(Int overridable, Arguments *args, Id_list *vars,
		Stmt_list *stmts)
{
    Prog *cnew = PMALLOC(compiler_pile, Prog, 1);

    /* default states */
    cnew->args = args;
    cnew->vars = vars;
    cnew->stmts = stmts;
    return cnew;
}

Arguments *arguments(Id_list *ids, char *rest)
{
    Arguments *cnew = PMALLOC(compiler_pile, Arguments, 1);

    cnew->ids = ids;
    cnew->rest = rest;
    return cnew;
}

Stmt *comment_stmt(char *comment)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = COMMENT;
    cnew->lineno = cur_lineno();
    cnew->u.comment = comment;
    return cnew;
}

Stmt *noop_stmt(void)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = NOOP;
    cnew->lineno = cur_lineno();
    return cnew;
}

Stmt *expr_stmt(Expr *expr)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = EXPR;
    cnew->lineno = cur_lineno();
    cnew->u.expr = expr;
    return cnew;
}

Stmt *compound_stmt(Stmt_list *stmt_list)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = COMPOUND;
    cnew->lineno = cur_lineno();
    cnew->u.stmt_list = stmt_list;
    return cnew;
}

Stmt *if_stmt(Expr *cond, Stmt *body)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = IF;
    cnew->lineno = cur_lineno();
    cnew->u.if_.cond = cond;
    cnew->u.if_.true = body;
    return cnew;
}

/* Modifies: if_, to produce the if-else statement. */
Stmt *if_else_stmt(Stmt *if_, Stmt *false)
{
    if_->type = IF_ELSE;
    if_->u.if_.false = false;
    return if_;
}

Stmt *for_range_stmt(char *id, Expr *lower, Expr *upper, Stmt *body)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = FOR_RANGE;
    cnew->lineno = cur_lineno();
    cnew->u.for_range.var = id;
    cnew->u.for_range.lower = lower;
    cnew->u.for_range.upper = upper;
    cnew->u.for_range.body = body;
    return cnew;
}

Stmt *for_list_stmt(char *id, Expr *list, Stmt *body)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = FOR_LIST;
    cnew->lineno = cur_lineno();
    cnew->u.for_list.var = id;
    cnew->u.for_list.list = list;
    cnew->u.for_list.body = body;
    return cnew;
}

Stmt *while_stmt(Expr *cond, Stmt *body)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = WHILE;
    cnew->lineno = cur_lineno();
    cnew->u.while_.cond = cond;
    cnew->u.while_.body = body;
    return cnew;
}

Stmt *switch_stmt(Expr *expr, Case_list *cases)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = SWITCH;
    cnew->lineno = cur_lineno();
    cnew->u.switch_.expr = expr;
    cnew->u.switch_.cases = cases;
    return cnew;
}

Stmt *break_stmt(void)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = BREAK;
    cnew->lineno = cur_lineno();
    return cnew;
}

Stmt *continue_stmt(void)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = CONTINUE;
    cnew->lineno = cur_lineno();
    return cnew;
}

Stmt *return_stmt(void)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = RETURN;
    cnew->lineno = cur_lineno();
    return cnew;
}

Stmt *return_expr_stmt(Expr *expr)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = RETURN_EXPR;
    cnew->lineno = cur_lineno();
    cnew->u.expr = expr;
    return cnew;
}

Stmt *catch_stmt(Id_list *errors, Stmt *body, Stmt *handler)
{
    Stmt *cnew = PMALLOC(compiler_pile, Stmt, 1);

    cnew->type = CATCH;
    cnew->lineno = cur_lineno();
    cnew->u.ccatch.errors = errors;
    cnew->u.ccatch.body = body;
    cnew->u.ccatch.handler = handler;
    return cnew;
}

Expr *integer_expr(Long num)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = INTEGER;
    cnew->lineno = cur_lineno();
    cnew->u.num = num;
    return cnew;
}

Expr *float_expr(Float fnum)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = FLOAT;
    cnew->lineno = cur_lineno();
    cnew->u.fnum = fnum;
    return cnew;
}

Expr *string_expr(char *s)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = STRING;
    cnew->lineno = cur_lineno();
    cnew->u.str = s;
    return cnew;
}

Expr *objnum_expr(Long objnum)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = OBJNUM;
    cnew->lineno = cur_lineno();
    cnew->u.objnum = objnum;
    return cnew;
}

Expr *symbol_expr(char *symbol)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = SYMBOL;
    cnew->lineno = cur_lineno();
    cnew->u.symbol = symbol;
    return cnew;
}

Expr *error_expr(char *error)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = T_ERROR;
    cnew->lineno = cur_lineno();
    cnew->u.error = error;
    return cnew;
}

Expr *objname_expr(char *name)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = OBJNAME;
    cnew->lineno = cur_lineno();
    cnew->u.name = name;
    return cnew;
}

Expr *var_expr(char *name)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = VAR;
    cnew->lineno = cur_lineno();
    cnew->u.name = name;
    return cnew;
}

Expr *assign_expr(Expr *lval, Expr *value)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = ASSIGN;
    cnew->lineno = cur_lineno();
    cnew->u.assign.lval = lval;
    cnew->u.assign.value = value;
    return cnew;
}

Expr *opt_expr(char *var, Expr *value)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = OPTIONAL_ASSIGN;
    cnew->lineno = cur_lineno();
    cnew->u.optassign.var = var;
    cnew->u.optassign.value = value;
    return cnew;
}

Expr *function_call_expr(char *name, Expr_list *args)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = FUNCTION_CALL;
    cnew->lineno = cur_lineno();
    cnew->u.function.name = name;
    cnew->u.function.args = args;
    return cnew;
}

Expr *pass_expr(Expr_list *args)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = PASS;
    cnew->lineno = cur_lineno();
    cnew->u.args = args;
    return cnew;
}

Expr *message_expr(Expr *to, char *message, Expr_list *args)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = CALL_METHOD;
    cnew->lineno = cur_lineno();
    cnew->u.message.to = (to) ? to : function_call_expr("this", NULL);
    cnew->u.message.name = message;
    cnew->u.message.args = args;
    return cnew;
}

Expr *expr_message_expr(Expr *to, Expr *message, Expr_list *args)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = EXPR_CALL_METHOD;
    cnew->lineno = cur_lineno();
    cnew->u.expr_message.to = (to) ? to : function_call_expr("this", NULL);
    cnew->u.expr_message.message = message;
    cnew->u.expr_message.args = args;
    return cnew;
}

Expr *list_expr(Expr_list *args)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = LIST;
    cnew->lineno = cur_lineno();
    cnew->u.args = args;
    return cnew;
}

Expr *dict_expr(Expr_list *args)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = DICT;
    cnew->lineno = cur_lineno();
    cnew->u.args = args;
    return cnew;
}

Expr *buffer_expr(Expr_list *args)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = BUFFER;
    cnew->lineno = cur_lineno();
    cnew->u.args = args;
    return cnew;
}

Expr *frob_expr(Expr *cclass, Expr *rep, Expr *handler)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = FROB;
    cnew->lineno = cur_lineno();
    cnew->u.frob.cclass = cclass;
    cnew->u.frob.rep = rep;
    cnew->u.frob.handler = handler;
    return cnew;
}

Expr *index_expr(Expr *list, Expr *offset)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = INDEX;
    cnew->lineno = cur_lineno();
    cnew->u.index.list = list;
    cnew->u.index.offset = offset;
    return cnew;
}

Expr *unary_expr(Int opcode, Expr *expr)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = UNARY;
    cnew->lineno = cur_lineno();
    cnew->u.unary.opcode = opcode;
    cnew->u.unary.expr = expr;
    return cnew;
}

Expr *binary_expr(Int opcode, Expr *left, Expr *right)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = BINARY;
    cnew->lineno = cur_lineno();
    cnew->u.binary.opcode = opcode;
    cnew->u.binary.left = left;
    cnew->u.binary.right = right;
    return cnew;
}

/* use doeq structure, it has all we need */
Expr *indecr_expr(Int opcode, char *var)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = INDECR;
    cnew->lineno = cur_lineno();
    cnew->u.doeq.opcode = opcode;
    cnew->u.doeq.var = var;
    cnew->u.doeq.value = NULL;
    return cnew;
}

Expr *doeq_expr(Int opcode, char *var, Expr *value)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = DOEQ;
    cnew->lineno = cur_lineno();
    cnew->u.doeq.opcode = opcode;
    cnew->u.doeq.var = var;
    cnew->u.doeq.value = value;
    return cnew;
}

Expr *and_expr(Expr *left, Expr *right)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = AND;
    cnew->lineno = cur_lineno();
    cnew->u.and.left = left;
    cnew->u.and.right = right;
    return cnew;
}

Expr *or_expr(Expr *left, Expr *right)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = OR;
    cnew->lineno = cur_lineno();
    cnew->u.or.left = left;
    cnew->u.or.right = right;
    return cnew;
}

Expr *cond_expr(Expr *cond, Expr *true, Expr *false)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = CONDITIONAL;
    cnew->lineno = cur_lineno();
    cnew->u.cond.cond = cond;
    cnew->u.cond.true = true;
    cnew->u.cond.false = false;
    return cnew;
}

Expr *map_expr(Expr *src, char *var, Expr *job, Int token)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = token;
    cnew->lineno = cur_lineno();
    cnew->u.map.src = src;
    cnew->u.map.var = var;
    cnew->u.map.job = job;
    return cnew;
}

Expr *map_range_expr(Expr *start, Expr *end, char *var, Expr *job, Int token)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = token;
    cnew->lineno = cur_lineno();
    cnew->u.maprange.start = start;
    cnew->u.maprange.end = end;
    cnew->u.maprange.var = var;
    cnew->u.maprange.job = job;
    return cnew;
}

Expr *critical_expr(Expr *expr)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = CRITICAL;
    cnew->lineno = cur_lineno();
    cnew->u.expr = expr;
    return cnew;
}

Expr *propagate_expr(Expr *expr)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = PROPAGATE;
    cnew->lineno = cur_lineno();
    cnew->u.expr = expr;
    return cnew;
}

Expr *splice_expr(Expr *expr)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = SPLICE;
    cnew->lineno = cur_lineno();
    cnew->u.expr = expr;
    return cnew;
}

Expr *range_expr(Expr *lower, Expr *upper)
{
    Expr *cnew = PMALLOC(compiler_pile, Expr, 1);

    cnew->type = RANGE;
    cnew->lineno = cur_lineno();
    cnew->u.range.lower = lower;
    cnew->u.range.upper = upper;
    return cnew;
}

Case_entry *case_entry(Expr_list *values, Stmt_list *stmts)
{
    Case_entry *cnew = PMALLOC(compiler_pile, Case_entry, 1);

    cnew->lineno = cur_lineno();
    cnew->values = values;
    cnew->stmts = stmts;
    return cnew;
}

Id_list *id_list(char *ident, Id_list *next)
{
    Id_list *cnew = PMALLOC(compiler_pile, Id_list, 1);

    cnew->lineno = cur_lineno();
    cnew->ident = ident;
    cnew->next = next;
    return cnew;
}

Stmt_list *stmt_list(Stmt *stmt, Stmt_list *next)
{
    Stmt_list *cnew = PMALLOC(compiler_pile, Stmt_list, 1);

    cnew->stmt = stmt;
    cnew->next = next;
    return cnew;
}

Expr_list *expr_list(Expr *expr, Expr_list *next)
{
    Expr_list *cnew = PMALLOC(compiler_pile, Expr_list, 1);

    cnew->expr = expr;
    cnew->next = next;
    return cnew;
}

Case_list *case_list(Case_entry *case_entry, Case_list *next)
{
    Case_list *cnew = PMALLOC(compiler_pile, Case_list, 1);

    cnew->case_entry = case_entry;
    cnew->next = next;
    return cnew;
}

/* Requires: prog is a program generated by the above constructors, and object
 *	     is the object the method will be defined on.
 * Modifies: May add entries to the strings table on object using
 *	     object_add_string().  May call compiler_error(), modifying the
 *	     error list.  Uses the instruction buffer.
 * Effects: Returns a method suitable for adding to an object with
 *	    object_add_method(), or NULL if there were errors. */
Method *generate_method(Prog *prog, Obj *object)
{
    /* Reset the error list counter to 0. */
    num_error_lists = 0;

    /* Compile the code into instr_buf. */
    instr_loc = 0;
    jump_loc = 0;
    the_prog = prog;
    compile_stmt_list(prog->stmts, -1, 0);

    /* If we have no errors, call final_pass() to make a method. */
    if (no_errors()) {
	code(RETURN);
	return final_pass(object);
    }

    return NULL;
}

/* Requires: Same as compile_stmt() below.
 * Modifies: Uses the instruction buffer and may call compiler_error().
 * Effects: Compiles the statements in stmt_list, in reverse order. */
static void compile_stmt_list(Stmt_list *stmt_list, Int loop, Int catch_level)
{
    if (stmt_list) {
	compile_stmt_list(stmt_list->next, loop, catch_level);
	compile_stmt(stmt_list->stmt, loop, catch_level);
    }
}

/* Requires:	loop is a destination number for the current loop, or -1 if
 *			we're not in a loop.
 *		catch_level is the number of catch statements we're inside in
 *			the current loop, if we're in a loop.
 * Modifies:	Uses the instruction buffer and may call compiler_error().
 * Effects:	compiles stmt into instr_buf. */
static void compile_stmt(Stmt *stmt, Int loop, Int catch_level)
{
    switch (stmt->type) {

      case COMMENT:

	/* Add a comment instruction and argument.  The interpreter will ignore
	 * this, but the decompiler will use it. */
	code(COMMENT);
	code_str(stmt->u.comment);

	break;

      case NOOP:

	/* Do nothing.  This is a dummy statement caused by a ';' character
	 * with no preceding expression.  Since the interpreter doesn't count
	 * statements, there's no need for a dummy instruction. */
	break;

      case EXPR:

	/* Compile the expression and code a POP opcode to discard its
	 * value. */
	compile_expr(stmt->u.expr);
	code(POP);

	break;

      case COMPOUND:

	/* Compile each statement in the statement list. */
	compile_stmt_list(stmt->u.stmt_list, loop, catch_level);

	break;

     case IF: {
	  Int end_dest = new_jump_dest();

	  /* Compile the condition expression. */
	  compile_expr(stmt->u.if_.cond);

	  /* Code an IF opcode with a jump argument pointing to the end of the
	   * true statmeent. */
	  code(IF);
	  code(end_dest);

	  /* Compile the true statement and set end_dest to the end of the
	   * false statement. */
	  compile_stmt(stmt->u.if_.true, loop, catch_level);
	  set_jump_dest_here(end_dest);
	  break;
      }

      case IF_ELSE: {
	  Int false_stmt_dest = new_jump_dest(), end_dest = new_jump_dest();

	  /* Compile the condition expression. */
	  compile_expr(stmt->u.if_.cond);

	  /* Code an IF_ELSE opcode with a jump argument pointing to the false
	   * statement. */
	  code(IF_ELSE);
	  code(false_stmt_dest);

	  /* Compile the true statement. */
	  compile_stmt(stmt->u.if_.true, loop, catch_level);

	  /* Code an ELSE opcode with a jump argument pointing to the end of
	   * the false statement. */
	  code(ELSE);
	  code(end_dest);

	  /* Set false_stmt_dest to here, and compile the false statement. */
	  set_jump_dest_here(false_stmt_dest);
	  compile_stmt(stmt->u.if_.false, loop, catch_level);

	  /* Set end_dest to the end of the false statement. */
	  set_jump_dest_here(end_dest);

	  break;
      }

      case FOR_RANGE: {
	  Int n, begin_dest = new_jump_dest(), end_dest = new_jump_dest();

	  /* Find the variable in the method's local variables. */
	  n = find_local_var(stmt->u.for_range.var);
	  if (n == -1) {
	      compiler_error(stmt->lineno, "%s is not a local variable",
			     stmt->u.for_range.var);
	      break;
	  }

	  /* Compile the lower and upper bounds of the range. */
	  compile_expr(stmt->u.for_range.lower);
	  compile_expr(stmt->u.for_range.upper);

	  /* Set begin_dest to here, and begin the loop with a FOR_RANGE
	   * opcode, with a jump argument pointing to the end of the loop. */
	  set_jump_dest_here(begin_dest);
	  code(FOR_RANGE);
	  code(end_dest);
	  code(n);

	  /* Compile the loop body. */
	  compile_stmt(stmt->u.for_range.body, begin_dest, 0);

	  /* Code an END opcode with a jump argument pointing to the beginning
	   * of the loop, and set end_dest. */
	  code(END);
	  code(begin_dest);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case FOR_LIST: {
	  Int n, begin_dest = new_jump_dest(), end_dest = new_jump_dest();

	  /* Find the variable in the method's local variables. */
	  n = find_local_var(stmt->u.for_list.var);
	  if (n == -1) {
	      compiler_error(stmt->lineno, "%s is not a local variable.",
			     stmt->u.for_list.var);
	      break;
	  }

	  /* Compile the list expression, and code a ZERO opcode to push a zero
	   * value onto the stack.  This will serve as the loop index. */
	  compile_expr(stmt->u.for_list.list);
	  code(ZERO);

	  /* Set begin_dest to here, and begin the loop with a FOR_LIST opcode,
	   * with a jump argument pointing to the end of the loop. */
	  set_jump_dest_here(begin_dest);
	  code(FOR_LIST);
	  code(end_dest);
	  code(n);

	  /* Compile the loop body. */
	  compile_stmt(stmt->u.for_list.body, begin_dest, 0);

	  /* Code an END opcode with a jump argument pointing to the beginning
	   * of the loop, and set end_dest. */
	  code(END);
	  code(begin_dest);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case WHILE: {
	  Int cond_dest = new_jump_dest(), begin_dest = new_jump_dest();
	  Int end_dest = new_jump_dest();

	  /* Set begin_dest to here, and compile the loop condition. */
	  set_jump_dest_here(cond_dest);
	  compile_expr(stmt->u.while_.cond);

	  /* Code a WHILE opcode with jump arguments pointing to the end of
	   * the loop and the beginning of the condition expression. */
	  set_jump_dest_here(begin_dest);
	  code(WHILE);
	  code(end_dest);
	  code(cond_dest);

	  /* Compile the loop body. */
	  compile_stmt(stmt->u.while_.body, begin_dest, 0);

	  /* Code an END opcode with a jump argument pointing to the beginning
	   * of the condition, and set end_dest. */
	  code(END);
	  code(cond_dest);
	  set_jump_dest_here(end_dest);
	  break;
      }

      case SWITCH: {
	  Int end_dest = new_jump_dest();
	  Case_list *cases;
	  Stmt_list *default_case_stmts = NULL;

	  /* Compile the switch expression. */
	  compile_expr(stmt->u.switch_.expr);

	  /* Set switch_dest to here, and code a SWITCH opcode with a jump
	   * argument pointing to the end of the switch statement.  The
	   * interpreter won't actually do anything with this instruction. */
	  code(SWITCH);
	  code(end_dest);

	  /* Pull out the default entry if there is one. */
	  cases = stmt->u.switch_.cases;
	  if (cases && !cases->case_entry->values) {
	      default_case_stmts = cases->case_entry->stmts;
	      cases = cases->next;
	  }

	  /* Compile the cases. */
	  compile_cases(cases, loop, catch_level, end_dest);

	  /* Code the final default case and set end_dest to the end of the
	   * switch statement.  We don't need a break opcode here under the
	   * current design of the switch statement. */
	  code(DEFAULT);
	  if (default_case_stmts)
	      compile_stmt_list(default_case_stmts, loop, catch_level);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case BREAK:

	if (loop == -1) {
	    compiler_error(stmt->lineno, "break statement outside loop.");
	    break;
	}

	/* Code a BREAK opcode, with a jump argument pointing to the loop
	 * instruction, and a cont of the number of catch statements we need to
	 * break out of.  The instruction after the loop instruction is always
	 * the end of the loop, and the interpreter can use the loop
	 * instruction to see if it needs to pop anything off the stack. */
	code(BREAK);
	code(loop);
	code(catch_level);
	break;

      case CONTINUE:

	if (loop == -1) {
	    compiler_error(stmt->lineno, "continue statement outside loop.");
	    break;
	}

	/* Code a CONTINUE opcode to bring us to the end of the loop.  The
	 * first argument to the opcode is the catch level, the number of catch
	 * statements we are breaking out of.  The second argument is the
	 * location of the loop opcode for the current loop.  The interpreter
	 * can use that to find the location of the end of the loop. */
	code(CONTINUE);
	code(loop);
	code(catch_level);
	break;

      case RETURN:

	/* Code a RETURN opcode. */
	code(RETURN);

	break;

      case RETURN_EXPR:

	/* Compile the expression to return and code a RETURN_EXPR opcode. */
	compile_expr(stmt->u.expr);
	code(RETURN_EXPR);

	break;

      case CATCH: {
	  Int handler_dest = new_jump_dest(), end_dest = new_jump_dest();

	  /* Increment the number of error lists we'll need, if there was an
	   * error list.  (No list is needed for a 'catch any'.) */
	  if (stmt->u.ccatch.errors)
	      num_error_lists++;

	  /* Code a CATCH opcode, with an error list argument, and a jump
	   * argument pointing to the handler. */
	  code(CATCH);
	  code(handler_dest);
	  code_errors(stmt->u.ccatch.errors);

	  /* Compile the body, incrementing catch_level. */
	  compile_stmt(stmt->u.ccatch.body, loop, catch_level + 1);

	  /* Code a CATCH_END opcode to end the catch statement, with a jump
	   * argument pointing to after the handler. */
	  code(CATCH_END);
	  code(end_dest);

	  /* Set handler_dest to here and compile the handler, if there is
	   * one. */
	  set_jump_dest_here(handler_dest);
	  if (stmt->u.ccatch.handler)
	      compile_stmt(stmt->u.ccatch.handler, loop, catch_level);

	  /* Code a HANDLER_END to signify that the handler is done. */
	  code(HANDLER_END);

	  /* Set end_dest to here, so the CATCH_END opcode can find it. */
	  set_jump_dest_here(end_dest);

	  break;
      }

    }
}

static void compile_cases(Case_list *cases, Int loop, Int catch_level,
			  Int end_dest)
{
    Int body_dest, next_dest;
    Case_entry *entry;
    Expr *expr;

    if (!cases)
	return;

    entry = cases->case_entry;
    if (!entry->values) {
	compiler_error(entry->lineno, "Default case not last in list.");
	return;
    }

    /* Compile previous cases. */
    compile_cases(cases->next, loop, catch_level, end_dest);

    /* It's a list of values.  Make a destination for the body and next
     * case. */
    body_dest = new_jump_dest();
    next_dest = new_jump_dest();

    /* Compile the list of values except for the last one. */
    compile_case_values(entry->values->next, body_dest);

    /* Compile the last value.  Code a LAST_CASE_RANGE or
     * LAST_CASE_VALUE instead of a CASE_RANGE or CASE_VALUE, and use
     * next_dest instead of body_dest for the jump argument. */
    expr = entry->values->expr;
    compile_expr(expr);
    code((expr->type == RANGE) ? LAST_CASE_RANGE : LAST_CASE_VALUE);
    code(next_dest);

    /* Set the body destination to here and compile the body. */
    set_jump_dest_here(body_dest);
    compile_stmt_list(entry->stmts, loop, catch_level);

    /* Code an END_SWITCH opcode with a jump argument pointing to the end of
     * the switch statement. */
    code(END_CASE);
    code(end_dest);

    /* Set next_dest here, which is where the next case will be
     * compiled. */
    set_jump_dest_here(next_dest);
}

static void compile_case_values(Expr_list *values, Int body_dest)
{
    if (values) {
	compile_case_values(values->next, body_dest);
	compile_expr(values->expr);
	code((values->expr->type == RANGE) ? CASE_RANGE : CASE_VALUE);
	code(body_dest);
    }
}

/* Modifies: Uses the instruction buffer and may call compiler_error().
 * Effects: Compiles the expressions in expr_list into instr_buf in reverse
 *	    order. */
static void compile_expr_list(Expr_list *expr_list)
{
    if (expr_list) {
	compile_expr_list(expr_list->next);
	compile_expr(expr_list->expr);
    }
}

/* Modifies: Uses the instruction buffer and may call compiler_error()
 * Effects: Compiles the recursive structure generated by list destructuring.
 */

/* temporary functions... */

static void compile_variable_assign (char *var)
{
    Int n;

    n = find_local_var(var);

    if (n != -1) {
	/* This is a local variable.  Code a SET_LOCAL opcode with a
	 * variable number argument. */
	code(SET_LOCAL);
	code(n);
    } else {
	/* This is an object variable.  Code a SET_OBJ_VAR opcode with
               * an identifier argument. */
	code(SET_OBJ_VAR);
	code_str(var);
    }
}

static void compile_optional_assign (char *var, Expr *e)
{
    Int end_dest = new_jump_dest();
    
    code (OPTIONAL_ASSIGN);
    code (end_dest);
    compile_expr (e);
    compile_variable_assign (var);
    set_jump_dest_here (end_dest);
}

static enum scatter_modes compile_assign (Expr *e, enum scatter_modes mode,
					  Bool in_scatter)
{
    Expr_list *lvalues;

    switch (e->type) {
    case VAR:
	/* This is ordinary parameter. Check if it's okay... */
	if (mode != SM_STANDARD) {
	    compiler_error(e->lineno,
			   "Standard argument following optional.");
	    return SM_ERROR;
	}

	compile_variable_assign(e->u.name);
	return SM_STANDARD;

    case LIST:
	if (mode != SM_STANDARD) {
	    compiler_error(e->lineno,
			   "Standard argument following optional or rest.");
	    return SM_ERROR;
	}

	/* This is recursive... */

	lvalues=e->u.args;

	code(SCATTER_START);
	mode=SM_STANDARD;
	if (!lvalues) {
	    compiler_error(e->lineno,
			   "Empty scatter assign list.");
	    return SM_ERROR;
	}
	/* First invert the list */
	{
	    Expr_list *ptr, *p1, *p2;
	    ptr=lvalues;
	    p1=ptr->next;
	    while (p1) {
		p2=p1;
 		p1=p2->next;
		p2->next=ptr;
		ptr=p2;
	    }
	    lvalues->next=NULL;
	    lvalues=ptr;
	}
	while (lvalues) {
	    if ((mode=compile_assign(lvalues->expr, mode, TRUE))
		== SM_ERROR)
		return SM_ERROR;
	    lvalues=lvalues->next;
	}
	code(SCATTER_END);
	return SM_STANDARD;

    case OPTIONAL_ASSIGN:

	if (!in_scatter) {
	    Int n;
	    n = find_local_var(e->u.optassign.var);
	    if (n != -1) {
              code(GET_LOCAL);
              code(n);
          } else {
              code(GET_OBJ_VAR);
              code_str(e->u.optassign.var);
          }
	}

	if (mode != SM_STANDARD && mode != SM_OPTIONAL) {
	    compiler_error(e->lineno,
			   "Optional argument following rest.");
	    return SM_ERROR;
	}

	compile_optional_assign(e->u.optassign.var, e->u.optassign.value);
	if (in_scatter)
	    code(OPTIONAL_END);
	return SM_OPTIONAL;

    case SPLICE:
	if (e->u.expr->type != VAR) {
	    compiler_error(e->lineno, "Parse error in the rest argument.");
	    return SM_ERROR;
	}
	code (SPLICE);
	compile_variable_assign(e->u.expr->u.name);
	return SM_REST;
    default:
	compiler_error(e->lineno, "Parse error in expression left of assignment.");
	return SM_ERROR;
    }
}

/* Modifies: Uses the instruction buffer and may call compiler_error().
 * Effects: Compiles expr into instr_buf. */
static void compile_expr(Expr *expr)
{
    switch(expr->type) {

      case INTEGER:

	/* Special-case zero and one to save space. */
	if (expr->u.num == 0) {
	    code(ZERO);
	} else if (expr->u.num == 1) {
	    code(ONE);
	} else {
	    code(INTEGER);
	    code(expr->u.num);
	}

	break;

      case FLOAT:

        code(FLOAT);
  	code(*((Long*)(&expr->u.fnum)));

  	break;
	
      case STRING:

	code(STRING);
	code_str(expr->u.str);

	break;

      case OBJNUM:

	code(OBJNUM);
	code(expr->u.objnum);

	break;

      case SYMBOL:

	code(SYMBOL);
	code_str(expr->u.symbol);

	break;

      case T_ERROR:

	code(T_ERROR);
	code_str(expr->u.error);

	break;

      case OBJNAME:

	code(OBJNAME);
	code_str(expr->u.name);

	break;

      case VAR: {
	  Int n;

	  /* Determine whether the variable is a local or global variable, and
	   * code the appropriate retrieval opcode. */
	  n = find_local_var(expr->u.name);
	  if (n != -1) {
	      code(GET_LOCAL);
	      code(n);
	  } else {
	      code(GET_OBJ_VAR);
	      code_str(expr->u.name);
	  }

	  break;
      }

      case ASSIGN:
          /* Use the ready-made function */
	  compile_expr(expr->u.assign.value);
          compile_assign(expr->u.assign.lval, SM_STANDARD, FALSE);
          break;

      case OPTIONAL_ASSIGN:
          compile_assign(expr, SM_STANDARD, FALSE);
          break;

      case FUNCTION_CALL: {
	  Int n;

	  code(START_ARGS);
	  compile_expr_list(expr->u.function.args);
	  n = find_function(expr->u.function.name);
	  if (n != -1) {
	      code(n);
	  } else {
	      compiler_error(expr->lineno, "Unknown function %s.",
			     expr->u.function.name);
	  }

	  break;
      }

      case PASS:

	code(START_ARGS);
	compile_expr_list(expr->u.args);
	code(PASS);

	break;

      case CALL_METHOD:

	compile_expr(expr->u.message.to);
	code(START_ARGS);
	compile_expr_list(expr->u.message.args);
	code(CALL_METHOD);
	code_str(expr->u.message.name);

	break;

      case EXPR_CALL_METHOD:

	compile_expr(expr->u.expr_message.to);
	compile_expr(expr->u.expr_message.message);
	code(START_ARGS);
	compile_expr_list(expr->u.expr_message.args);
	code(EXPR_CALL_METHOD);

	break;

      case LIST: {
	  Expr_list **elistp = &expr->u.args, *elist;

	  /* [@foo, ...] --> foo [...] SPLICE_ADD */
	  while (*elistp && (*elistp)->next)
	      elistp = &(*elistp)->next;
	  if (*elistp && (*elistp)->expr->type == SPLICE) {
	      /* Compile the spliced expression. */
	      compile_expr((*elistp)->expr->u.expr);

	      /* Make a list out of the remaining expressions. */
	      code(START_ARGS);
	      elist = *elistp;
	      *elistp = NULL;
	      compile_expr_list(expr->u.args);
	      *elistp = elist;
	      code(LIST);

	      /* Add them together with SPLICE_ADD. */
	      code(SPLICE_ADD);
	      break;
	  }

	  /* The general case; just code up the arguments and make a list. */
	  code(START_ARGS);
	  compile_expr_list(expr->u.args);
	  code(LIST);
	  break;
      }

      case DICT:

	code(START_ARGS);
	compile_expr_list(expr->u.args);
	code(DICT);

	break;

      case BUFFER:

	code(START_ARGS);
	compile_expr_list(expr->u.args);
	code(BUFFER);

	break;

      case FROB:

        if (!expr->u.frob.handler) {
	    compile_expr(expr->u.frob.cclass);
	    compile_expr(expr->u.frob.rep);
	    code(FROB);
	} else {
	    compile_expr(expr->u.frob.cclass);
	    compile_expr(expr->u.frob.rep);
	    compile_expr(expr->u.frob.handler);
	    code(OP_HANDLED_FROB);
	}
	break;

      case INDEX:

	compile_expr(expr->u.index.list);
	compile_expr(expr->u.index.offset);
	code(INDEX);

	break;

      case UNARY:

	compile_expr(expr->u.unary.expr);
	code(expr->u.unary.opcode);

	break;

      case BINARY:

	compile_expr(expr->u.binary.left);
	compile_expr(expr->u.binary.right);
	code(expr->u.binary.opcode);

	break;

      case INDECR: {
          Int n;

          n = find_local_var(expr->u.doeq.var);
          if (n != -1) {
              code(GET_LOCAL);
              code(n);
              code(expr->u.doeq.opcode);
              code(SET_LOCAL);
              code(n);
          } else {
              code(GET_OBJ_VAR);
              code_str(expr->u.doeq.var);
              code(expr->u.doeq.opcode);
              code(SET_OBJ_VAR);
              code_str(expr->u.doeq.var);
          }
          break;
      }

      case DOEQ: {
          Expr *value = expr->u.doeq.value;
          Int n, otherop = 0;

          n = find_local_var(expr->u.doeq.var);

          /* do a touch of optimization, pfeh */
          if (expr->u.doeq.opcode == PLUS_EQ) {
              if ((value->type == INTEGER && value->u.num == 1) ||
                  (value->type == FLOAT && value->u.fnum == 1.0)) {
                  otherop = P_INCREMENT;
              }
          } else if (expr->u.doeq.opcode == MINUS_EQ) {
              if ((value->type == INTEGER && value->u.num == -1) ||
                  (value->type == FLOAT && value->u.fnum == -1.0)) {
                  otherop = P_DECREMENT;
              }
          }

          if (!otherop)
              compile_expr(value);

          if (n != -1) {
              code(GET_LOCAL);
              code(n);
              if (otherop)
                  code(otherop);
              else
                  code(expr->u.doeq.opcode);
              code(SET_LOCAL);
              code(n);
          } else {
              code(GET_OBJ_VAR);
              code_str(expr->u.doeq.var);
              if (otherop)
                  code(otherop);
              else
                  code(expr->u.doeq.opcode);
              code(SET_OBJ_VAR);
              code_str(expr->u.doeq.var);
          }
          break;
      }

      case AND: {
	  Int end_dest = new_jump_dest();

	  /* Compile ther left-hand expression. */
	  compile_expr(expr->u.and.left);

	  /* Code an AND opcode with a jump argument pointing to the end of the
	   * expression. */
	  code(AND);
	  code(end_dest);

	  /* Compile the right-hand expression and set end_dest. */
	  compile_expr(expr->u.and.right);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case OR: {
	  Int end_dest = new_jump_dest();

	  /* Compile the left-hand expression. */
	  compile_expr(expr->u.and.left);

	  /* Code an OR opcode with a jump argument pointing to the end of the
	   * expression. */
	  code(OR);
	  code(end_dest);

	  /* Compile the right-hand expression and set end_dest. */
	  compile_expr(expr->u.and.right);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case CONDITIONAL: {
	  Int false_dest = new_jump_dest(), end_dest = new_jump_dest();

	  /* Compile the condition expression. */
	  compile_expr(expr->u.cond.cond);

	  /* Code a CONDITIONAL opcode with a jump argument pointing to the
	   * false expression. */
	  code(CONDITIONAL);
	  code(false_dest);

	  /* Compile the true expression. */
	  compile_expr(expr->u.cond.true);

	  /* Code an ELSE opcode with a jump argument pointing to the end of
	   * the expression. */
	  code(ELSE);
	  code(end_dest);

	  /* Set false_dest to here and compile the false expression. */
	  set_jump_dest_here(false_dest);
	  compile_expr(expr->u.cond.false);

	  /* Set end_dest to here. */
	  set_jump_dest_here(end_dest);

	  break;
      }

      case OP_MAPHASH:
      case OP_FILTER:
      case OP_FIND:
      case OP_MAP: {
	  Int n, begin_dest = new_jump_dest(), end_dest = new_jump_dest();

	  /* Find the variable in the method's local variables. */
	  n = find_local_var(expr->u.map.var);
	  if (n == -1) {
	      compiler_error(expr->lineno, "%s is not a local variable.",
			     expr->u.map.var);
	      break;
	  }

	  /* Compile the list expression, and code a ZERO opcode to push a zero
	   * value onto the stack.  This will serve as the loop index. */
	  compile_expr(expr->u.map.src);
	  code(ZERO);

	  /* a zero for the returned list */
	  code(ZERO);

	  /* another ZERO for the initial returned value */
	  code(ZERO);

	  /* Set begin_dest to here, and begin the loop with a FOR_LIST opcode,
	   * with a jump argument pointing to the end of the loop. */
	  set_jump_dest_here(begin_dest);
	  code(expr->type);
	  code(end_dest);
	  code(n);

	  /* Compile the loop body. */
	  compile_expr(expr->u.map.job);

	  /* Code an END opcode with a jump argument pointing to the beginning
	   * of the loop, and set end_dest. */
	  code(END);
	  code(begin_dest);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case OP_MAPHASH_RANGE:
      case OP_FILTER_RANGE:
      case OP_FIND_RANGE:
      case OP_MAP_RANGE: {
	  Int n, begin_dest = new_jump_dest(), end_dest = new_jump_dest();

	  /* Find the variable in the method's local variables. */
	  n = find_local_var(expr->u.maprange.var);
	  if (n == -1) {
	      compiler_error(expr->lineno, "%s is not a local variable.",
			     expr->u.maprange.var);
	      break;
	  }

	  /* compile the start and the end of the loop */ 
	  compile_expr(expr->u.maprange.start);
	  compile_expr(expr->u.maprange.end);

	  /* ZERO for the range counter */
	  code(ZERO);

	  /* another ZERO for the initial returned value */
	  code(ZERO);

	  /* Set begin_dest to here, and begin the loop with a FOR_LIST opcode,
	   * with a jump argument pointing to the end of the loop. */
	  set_jump_dest_here(begin_dest);
	  code(expr->type);
	  code(end_dest);
	  code(n);

	  /* Compile the loop body. */
	  compile_expr(expr->u.maprange.job);

	  /* Code an END opcode with a jump argument pointing to the beginning
	   * of the loop, and set end_dest. */
	  code(END);
	  code(begin_dest);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case CRITICAL: {
	  Int end_dest = new_jump_dest();

	  /* Code a CRITICAL opcode with a jump argument pointing to the end of
	   * the critical expression. */
	  code(CRITICAL);
	  code(end_dest);

	  /* Compile the critical expression. */
	  compile_expr(expr->u.expr);

	  /* Code a CRITICAL_END opcode, and set end_dest. */
	  code(CRITICAL_END);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case PROPAGATE: {
	  Int end_dest = new_jump_dest();

	  /* Code a PROPAGATE opcode with a jump argument pointing to the end
	   * of the critical expression. */
	  code(PROPAGATE);
	  code(end_dest);

	  /* Compile the critical expression. */
	  compile_expr(expr->u.expr);

	  /* Code a PROPAGATE_END opcode, and set end_dest. */
	  code(PROPAGATE_END);
	  set_jump_dest_here(end_dest);

	  break;
      }

      case SPLICE:

	compile_expr(expr->u.expr);
	code(SPLICE);

	break;

      case RANGE:

	compile_expr(expr->u.range.lower);
	compile_expr(expr->u.range.upper);

	break;
    }
}

/* Effects: Returns the number of id as a local variable, or -1 if it doesn't
 *	    match any of the local variable names. */
static Int find_local_var(char *id)
{
    Int count = 0;
    Id_list *id_list;

    /* Check arguments. */
    for (id_list = the_prog->args->ids; id_list; id_list = id_list->next) {
	count++;
	if (strcmp(id_list->ident, id) == 0)
	    return id_list_size(the_prog->args->ids) - count;
    }

    /* Check variable argument container, if it exists. */
    if (the_prog->args->rest) {
	if (strcmp(the_prog->args->rest, id) == 0)
	    return count;
	count++;
    }

    /* Check local variables. */
    for (id_list = the_prog->vars; id_list; id_list = id_list->next) {
	if (strcmp(id_list->ident, id) == 0)
	    return count;
	count++;
    }

    /* No match. */
    return -1;
}

/* Requires: Works inefficiently if pos is much larger than instr_size.
 * Modifies: instr_buf, instr_size.
 * Effects: Roughly doubles the size of the instruction buffer until it can
 *	    hold an instruction at the location specified by pos. */
static void check_instr_buf(Int pos)
{
    while (pos >= instr_size) {
	instr_size = instr_size * 2 + MALLOC_DELTA;
	instr_buf = EREALLOC(instr_buf, Instr, instr_size);
    }
}

/* Modifies: instr_buf, instr_loc, maybe instr_size.
 * Effects: Appends the Long integer val to the instruction buffer. */
static void code(Long val)
{
    check_instr_buf(instr_loc);
    instr_buf[instr_loc++].val = val;
}

/* Requires: str should be good until the next pfree() of compiler_pile, at
 *	     least.  Putting it in the instruction buffer doesn't cause it to
 *	     be freed.
 * Modifies: instr_buf, instr_loc, maybe instr_size.
 * Effects: Appends the string str to the instruction buffer. */
static void code_str(char *str)
{
    check_instr_buf(instr_loc);
    instr_buf[instr_loc++].str = str;
}

/* Requires: errors should be good until the next pfree() of compiler_pile, at
 *	     least.  Putting it in the instruction buffer doesn't cause it to
 *	     be freed.
 * Modifies: instr_buf, instr_loc, maybe instr_size.
 * Effects: Appends errors to the instruction buffer. */
static void code_errors(Id_list *errors)
{
    check_instr_buf(instr_loc);
    instr_buf[instr_loc++].errors = errors;
}

/* Modifies: jump_loc, maybe jump_size and jump_table.
 * Effects: Returns a location in the jump table for use as a destination. */
static Int new_jump_dest(void)
{
    /* Resize jump table if we need to. */
    if (jump_loc == jump_size) {
	jump_size = jump_size * 2 + MALLOC_DELTA;
	jump_table = EREALLOC(jump_table, Int, jump_size);
    }

    /* Increment jump_loc, and return the old value. */
    return jump_loc++;
}

/* Requires: dest was obtained with new_jump_dest() during this compile.
 *	     pos is a valid instruction in the instruction stream.
 * Modifies: Contents of jump_table.
 * Effects: Sets the destination numbered by dest to the current location. */
static void set_jump_dest_here(Int dest)
{
    jump_table[dest] = instr_loc;
}

static Int id_list_size(Id_list *id_list)
{
    Int count = 0;

    for (; id_list; id_list = id_list->next)
	count++;

    return count;
}

/* Requires: The instruction buffer is full of code.  The method did not have
 *	     any errors.  the_prog->vars is what it originally was.
 * Modifies: Adds global identifiers, adds strings to object.
 * Effects: Converts the data in the instruction buffer into a method. */
static Method *final_pass(Obj *object)
{
    Method * method;
    Id_list  * idl;
    Op_info  * info;
    cStr * string;
    Int i, j, opcode, arg_type, cur_error_list;

    method = EMALLOC(Method, 1);
    method->m_flags  = MF_NONE;
    method->m_access = MS_PUBLIC;
    method->native   = -1;

    /* Set argument names. */
    method->num_args = id_list_size(the_prog->args->ids);
    if (method->num_args) {
	method->argnames = TMALLOC(Int, method->num_args);
	i = 0;
	for (idl = the_prog->args->ids; idl; idl = idl->next)
	    method->argnames[i++] = object_add_ident(object, idl->ident);
    }

    /* Set rest. */
    if (the_prog->args->rest)
	method->rest = object_add_ident(object, the_prog->args->rest);
    else
	method->rest = -1;

    /* Set variable names. */
    method->num_vars = id_list_size(the_prog->vars);
    if (method->num_vars) {
	method->varnames = TMALLOC(Int, method->num_vars);
	i = 0;
	for (idl = the_prog->vars; idl; idl = idl->next)
	    method->varnames[i++] = object_add_ident(object, idl->ident);
    }

    /* Allocate space for error lists, and initialize cur_error_list. */
    method->num_error_lists = num_error_lists;
    if (num_error_lists)
	method->error_lists = TMALLOC(Error_list, num_error_lists);
    cur_error_list = 0;

    /* Copy the opcodes, translating from intermediate instruction forms. */
    method->opcodes = TMALLOC(Long, instr_loc);
    method->num_opcodes = instr_loc;
    i = 0;
    while (i < instr_loc) {
	opcode = method->opcodes[i] = instr_buf[i].val;

	/* Use opcode info table for anything else. */
	info = &op_table[opcode];
	for (j = 0; j < 2; j++) {
	    arg_type = (j == 0) ? info->arg1 : info->arg2;
	    if (arg_type) {
		i++;
		switch (arg_type) {
		  case INTEGER:
		  case VAR:
		    method->opcodes[i] = instr_buf[i].val;
		    break;

		  case STRING:
		    string = string_from_chars(instr_buf[i].str,
					       strlen(instr_buf[i].str));
		    method->opcodes[i] = object_add_string(object, string);
		    string_discard(string);
		    break;

		  case IDENT:
		    method->opcodes[i] = object_add_ident(object,
							  instr_buf[i].str);
		    break;

		  case JUMP:
		    method->opcodes[i] = jump_table[instr_buf[i].val];
		    break;

		  case T_ERROR: {
		      Int count;
		      Int *ids;

		      if (!instr_buf[i].errors) {
			  /* This is a 'catch any'.  Just code a -1. */
			  method->opcodes[i] = -1;
			  break;
		      }

		      /* Count the number of error codes to catch. */
		      count = 0;
		      for (idl = instr_buf[i].errors; idl; idl = idl->next)
			  count++;

		      /* Allocate space in an error list. */
		      ids = TMALLOC(Int, count);
		      method->error_lists[cur_error_list].num_errors = count;
		      method->error_lists[cur_error_list].error_ids = ids;

		      /* Store the error ids. */
		      count = 0;
		      for (idl = instr_buf[i].errors; idl; idl = idl->next)
			  ids[count++] = ident_get(idl->ident);

		      method->opcodes[i] = cur_error_list++;
		      break;
		  }
		}
	    }
	}
	i++;
    }

    method->refs = 1;
    return method;
}

