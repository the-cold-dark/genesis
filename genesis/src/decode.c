/*
// Full copyright information is available in the file ../doc/CREDITS
//
// File: decode.c
// ---
// Routines to decompile a method.
*/

#define _DECODE_C_
#include "defs.h"

#include "cdc_pcode.h"
#include "util.h"

#define TOKEN_SIZE (sizeof(binary_tokens) / sizeof(*binary_tokens))
#define PREC_SIZE (sizeof(precedences) / sizeof(*precedences))
#define A_VERY_LARGE_PRECEDENCE_LEVEL	1000

#define COMPLEX_FLAG			0x1
#define SPANNING_IF_FLAG		0x2
#define COMPLEX_IF_FLAG			0x4

/* this hack is for the sole purpose of assignments as expressions decompiling
   correctly */
#define PAREN_ASSIGN 1
#define NOPAREN_ASSIGN 0

typedef struct context Context;

struct context {
    Short type;
    Int end;
    Context *enclosure;
};

INTERNAL Int count_lines(Int start, Int end, unsigned *flags);
INTERNAL Stmt_list *decompile_stmt_list(Int start, Int end);
INTERNAL Stmt *decompile_stmt(Int *pos_ptr);
INTERNAL Stmt *decompile_body(Int start, Int end);
INTERNAL Stmt *decompile_until(Int *start, Int marker);
INTERNAL Stmt *body_from_stmt_list(Stmt_list *stmts);
INTERNAL Case_list *decompile_cases(Int start, Int end);
INTERNAL Case_entry *decompile_case(Int *pos_ptr, Int switch_end);
INTERNAL Expr_list *decompile_case_values(Int *pos_ptr, Int *end_ret);
INTERNAL Id_list *make_error_id_list(Int which);
INTERNAL Expr_list *decompile_expressions(Int *pos_ptr);
INTERNAL Expr_list *decompile_expressions_bounded(Int *pos_ptr, Int end);
INTERNAL cList *unparse_stmt_list(cList *output, Stmt_list *stmts, Int indent);
INTERNAL cList *unparse_stmt(cList *output, Stmt *stmt, Int indent, Stmt *last);
INTERNAL Int is_complex_if_else_stmt(Stmt *stmt);
INTERNAL Int is_complex_type(Int type);
INTERNAL cList *unparse_body(cList *output, Stmt *body, cStr *str, Int indent);
INTERNAL cList *unparse_cases(cList *output, Case_list *cases, Int indent);
INTERNAL cList *unparse_case(cList *output, Case_entry *case_entry, Int indent);
INTERNAL cStr *unparse_expr(cStr *str, Expr *expr, Int paren);
INTERNAL Int is_this(Expr *expr);
INTERNAL cStr *unparse_args(cStr *str, Expr_list *args);
INTERNAL cStr *unparse_expr_prec(cStr *str, Expr *expr, Int caller_type,
				 Int assoc);
INTERNAL cStr *unparse_complex_expr(cStr *str, Expr *expr, Int caller_type,
				  Int assoc);
INTERNAL Int prec_level(Int opcode);
INTERNAL char *binary_token(Int opcode);
INTERNAL cList *add_and_discard_string(cList *output, cStr *str);
INTERNAL char *varname(Int ind);

/* These globals get set at the start and are never modified. */
INTERNAL Obj *the_object;
INTERNAL Method *the_method;
INTERNAL Long *the_opcodes;
INTERNAL Int the_increment;

static struct {
    Int opcode;
    char *token;
} binary_tokens[] = {
    { OP_IN,	"in" },
    { EQ,	"==" },
    { NE,	"!=" },
    { '>',	">" },
    { GE,	">=" },
    { '<',	"<" },
    { LE,	"<=" },
    { '+',	"+" },
    { '-',	"-" },
    { '*',	"*" },
    { '/',	"/" },
    { '%',	"%" },
};

static struct {
    Int opcode;
    Int level;
} precedences[] = {
    { OP_ASSIGN,	 1 },
    { PLUS_EQ,		 1 },
    { MINUS_EQ,		 1 },
    { MULT_EQ,		 1 },
    { DIV_EQ,		 1 },
    { OPTIONAL_ASSIGN,   1 },
    { CONDITIONAL,	 2 },
    { OR,		 3 },
    { AND,		 4 },
    { OP_IN,		 5 },
    { EQ,		 6 },
    { NE,		 6 },
    { '>',		 6 },
    { GE,		 6 },
    { '<',		 6 },
    { LE,		 6 },
    { '+',		 7 },
    { '-',		 7 },
    { '*',		 8 },
    { '/',		 8 },
    { '%',		 8 },
    { '!',		 9 },
    { NEG,		 9 },
    { P_INCREMENT,	10 },
    { P_DECREMENT,	10 },
    { INCREMENT,	10 },
    { DECREMENT,	10 },
    { CALL_METHOD,	11 },
    { INDEX,		12 }
};

Int line_number(Method *method, Int pc) {
    Int count = 1;
    unsigned flags;

#if DISABLED
    /* Count declaration lines. */
    if (method->m_flags & MF_NOOVER)
	count++;
#endif

    if (method->num_args || method->rest != -1)
	count++;
    if (method->num_vars)
	count++;
    if (count > 1)
	count++;

    the_opcodes = method->opcodes;
    return count + count_lines(0, pc, &flags);
}

INTERNAL Int count_lines(Int start, Int end, unsigned *flags)
{
    Int count=0, last=-1, next, complex_catch=0, with_start=0;

    *flags = 0x0;

    while (start < end) {

	/* Find next opcode. */
	next = start + 1;
	if (op_table[the_opcodes[start]].arg1)
	    next++;
	if (op_table[the_opcodes[start]].arg2)
	    next++;

	/* Check for statement opcodes. */
	switch (the_opcodes[start]) {

	  case POP:
	  case BREAK:
	  case CONTINUE:
	  case RETURN:
	  case RETURN_EXPR:
	    /* Count one line. */
	    count++;
	    if (last != -1)
		*flags |= COMPLEX_FLAG;
	    *flags &= ~SPANNING_IF_FLAG;
	    last = the_opcodes[start];
	    break;

	  case COMMENT:
	    /* If last is a comment or this is the beginning of a block,
	     * increment once; otherwise, increment twice. */
	    count += ((last == -1 || last == COMMENT) ? 1 : 2);
	    if (last != -1)
		*flags |= COMPLEX_FLAG;
	    *flags &= ~SPANNING_IF_FLAG;
	    last = COMMENT;
	    break;

	  case IF_ELSE: {
	      Int body_end;
	      unsigned if_flags = 0x0, else_flags = 0x0;

	      /* Count "if (expr)" line and consider if body. */
	      count++;
	      body_end = the_opcodes[start + 1] - 2;
	      start = next;

	      if (end < body_end) {
		  /* End is inside if body. */
		  return count + count_lines(start, end, &if_flags);
	      } else {
		  /* Count if body. */
		  count += count_lines(start, body_end, &if_flags);
	      }

	      /* Count "else" line and consider else body. */
	      count++;
	      start = body_end + 2;
	      body_end = the_opcodes[start - 1];

	      if (end < body_end) {
		  /* End is in else body.  Count the entire else body to see if
		   * it contains a spanning if; if so, adjust count backwards
		   * by one to account for the "if (expr)" being on the same
		   * line as our "else".  Then count the body up to the end. */
		  count_lines(start, body_end, &else_flags);
		  if (else_flags & SPANNING_IF_FLAG)
		      count--;
		  return count + count_lines(start, end, &else_flags);
	      }

	      /* Count else body. */
	      count += count_lines(start, body_end, &else_flags);

	      if (else_flags & SPANNING_IF_FLAG) {
		  /* Adjust the count back one, since the "if (expr)" line went
		   * on the same line as our "else".  Also, if the else body
		   * added a line for a closing brace, adjust back one more,
		   * since the responsibility for the closing brace falls on
		   * the outermost if-else. */
		  count--;
		  if (else_flags & COMPLEX_IF_FLAG)
		      count--;
	      }

	      /* Add a line for the closing brace if the if body was complex,
	       * if the else body was complex and not a spanning if, or if the
	       * else body was a spanning if and had complex subclauses.  Set
	       * COMPLEX_IF_FLAG if we do this. */
	      if ((if_flags & COMPLEX_FLAG) ||
		  (else_flags & COMPLEX_FLAG &&
		   !(else_flags & SPANNING_IF_FLAG)) ||
		  (else_flags & SPANNING_IF_FLAG &&
		   else_flags & COMPLEX_IF_FLAG)) {
		  /* Boy, that was a messy condition. */
		  count++;
		  *flags |= COMPLEX_IF_FLAG;
	      }

	      /* Set other flags. */
	      *flags |= COMPLEX_FLAG;
	      if (last == -1)
		  *flags |= SPANNING_IF_FLAG;

	      last = IF_ELSE;
	      next = body_end;
	      break;
	  }

	  case IF:
	  case FOR_RANGE:
	  case FOR_LIST:
	  case WHILE:
	  case LAST_CASE_VALUE:
	  case LAST_CASE_RANGE: {
	      Int opcode = the_opcodes[start], body_end;
	      unsigned body_flags;

	      /* Count header line and consider body. */
	      count++;
	      body_end = the_opcodes[start + 1];
	      if (opcode == LAST_CASE_VALUE || opcode == LAST_CASE_RANGE)
		  body_end -= 2;
	      start = next;

	      if (end < body_end) {
		  /* End is in body. */
		  return count + count_lines(start, end, &body_flags);
	      }

	      /* Count body.  If it's complex, count line for closing brace. */
	      count += count_lines(start, body_end, &body_flags);
	      if (opcode != LAST_CASE_VALUE && opcode != LAST_CASE_RANGE) {
		  if (body_flags & COMPLEX_FLAG)
		      count++;
	      }

	      /* For if statements, set COMPLEX_IF_FLAG if body is complex, and
	       * set SPANNING_IF_FLAG if this is the first statement (it may be
	       * unset by a later statement). */
	      if (opcode == IF) {
		  if (body_flags & COMPLEX_FLAG)
		      *flags |= COMPLEX_IF_FLAG;
		  if (last == -1)
		      *flags |= SPANNING_IF_FLAG;
	      } else {
		  *flags &= ~SPANNING_IF_FLAG;
	      }

	      *flags |= COMPLEX_FLAG;
	      last = opcode;
	      next = body_end;
	      break;
	  }

	  case SWITCH: {
	      Int body_end;
	      unsigned body_flags;

	      /* Count switch header and consider body. */
	      count++;
	      body_end = the_opcodes[start + 1];
	      start += 2;

	      if (end < body_end) {
		  /* End is in switch body. */
		  return count + count_lines(start, end, &body_flags);
	      }

	      /* Count switch body recursively (so that default case knows
	       * where to stop).  If the default case was complex, count the
	       * closing brace for the default case. */
	      count += count_lines(start, body_end, &body_flags);

	      /* Count closing brace and set flags. */
	      count++;
	      *flags |= COMPLEX_FLAG;
	      *flags &= ~SPANNING_IF_FLAG;
	      last = SWITCH;
	      next = body_end;
	      break;
	  }

	  case DEFAULT: {
	      unsigned body_flags;

	      /* Switch bodies are counted recursively, so the end of the
	       * default case is end.  Count default header and body. */
	      count += 1 + count_lines(next, end, &body_flags);
	      next = end;
	      break;
	  }

	  case CATCH: {
	      Int body_end, body_lines;
	      unsigned body_flags;

	      /* Count catch line and consider body. */
	      count++;
	      body_end = the_opcodes[start + 1];
	      start = next;

	      if (end < body_end) {
		  /* end is inside catch body. */
		  return count + count_lines(start, end, &body_flags);
	      }

	      /* Count body and closing brace, if applicable */
              body_lines = count_lines(start, body_end, &body_flags);
              count += body_lines;
              if (body_lines > 1) {
	          count += 1;
                  complex_catch++;
              }

              /*
              // BUGNOTE: this odd handling of with_start etc does not
              // take into account nested catch statements, which will
              // cause the line count to be off.  It does work for the
              // non-nested catch, which is fine until I rewrite this.
              // -B
              */
	      if (the_opcodes[body_end] == HANDLER_END) {
		  /* Skip dummy HANDLER_END instruction. */
		  last = CATCH;
		  next = body_end + 1;
                  with_start=0;
	      } else {
		  /* We don't know where the end of the handler is.  Rather
		   * than counting it recursively, fake it, and count the
		   * closing brace when we hit the HANDLER_END. */
		  last = -1;
		  next = body_end;

                  /* add in the line for } with { */
                  count++;

                  /* remember where we are */
                  with_start=count;
	      }

	      *flags &= ~SPANNING_IF_FLAG;
	      *flags |= COMPLEX_FLAG;
	      break;
	  }

	  case HANDLER_END:
	    /* Count closing brace, and set last to CATCH to reflect the
	     * enclosing catch statement. */
            if ((count - with_start) > 1 || complex_catch) {
	        count++;
                complex_catch--;
                with_start = 0;
            }
	    last = CATCH;
	    break;
	}

	start = next;
    }

    return count;
}

cList *decompile(Method *method, Obj *object, Int increment, int fflags)
{
    Stmt_list *body;
    cList *output;
    cStr *str;
    Int i;
    char *s;

    /* Set globals so we don't have to pass method and object around. */
    the_object = object;
    the_method = method;
    the_opcodes = method->opcodes;
    the_increment = increment;
    format_flags = fflags;

    /* Prepare output list. */
    output = list_new(0);

    /* Add 'args' line if there are arguments. */
    if (method->num_args || method->rest != -1) {
	str = string_from_chars("arg ", 4);
	for (i = method->num_args - 1; i >= 0; i--) {
	    s = ident_name(object_get_ident(object, method->argnames[i]));
	    str = string_add_chars(str, s, strlen(s));
	    if (i > 0 || method->rest != -1)
		str = string_add_chars(str, ", ", 2);
	}
	if (method->rest != -1) {
	    str = string_addc(str, '@');
	    s = ident_name(object_get_ident(object, method->rest));
	    str = string_add_chars(str, s, strlen(s));
	}
	str = string_addc(str, ';');
	output = add_and_discard_string(output, str);
    }

    /* Add 'vars' line if there are variables. */
    if (method->num_vars) {
	str = string_from_chars("var ", 4);
	for (i = method->num_vars - 1; i >= 0; i--) {
	    s = ident_name(object_get_ident(object, method->varnames[i]));
	    str = string_add_chars(str, s, strlen(s));
	    if (i > 0)
		str = string_add_chars(str, ", ", 2);
	}
	str = string_addc(str, ';');
	output = add_and_discard_string(output, str);
    }

    /* Add blank line if there were declarations and there is code. */
    if (output->len && method->num_opcodes)
	output = add_and_discard_string(output, string_new(0));

    /* Decompile opcodes Into parse tree. */
    body = decompile_stmt_list(0, method->num_opcodes);

    /* The first statement in the body (the last statement in the method) will
     * be a 'return 0' statement.  Ignore this. */
    body = body->next;

    /* Now unparse the body, add it to the list, and return the output. */
    output = unparse_stmt_list(output, body, 0);

    /* Free up all that memory we've been allocating and losing track of. */
    pfree(compiler_pile);

    return output;
}

static Stmt *decompile_stmt(Int *pos_ptr)
{
    Int pos = *pos_ptr, end;
    Expr_list *exprs;
    Stmt *stmt, *body;
    char *var, *comment;

    /* Most statement opcodes follow at least one expression.  Therefore, the
     * first thing we do is decompile expressions until we hit a statement
     * opcode. */
    exprs = decompile_expressions(&pos);

    /* Now look at the statement opcode. */
    switch (the_opcodes[pos]) {

      case COMMENT:
	/* COMMENT opcode follows no expressions. */
	comment = string_chars(the_object->strings[the_opcodes[pos + 1]].str);
	(*pos_ptr) = pos + 2;
	return comment_stmt(comment);

      case POP:
	/* POP opcode follows one expression. */
	(*pos_ptr) = pos + 1;
	return expr_stmt(exprs->expr);

      case IF:
	/* IF opcode follows one expression. */
	end = the_opcodes[pos + 1];
	body = decompile_body(pos + 2, end);
	stmt = if_stmt(exprs->expr, body);
	(*pos_ptr) = end;
	return stmt;

      case IF_ELSE:
	/* IF_ELSE opcode follows one expression.  First get the if part. */
	end = the_opcodes[pos + 1];
	body = decompile_body(pos + 2, end - 2);
	stmt = if_stmt(exprs->expr, body);

	/* Now get the else part. */
	pos = end;
	end = the_opcodes[pos - 1];
	body = decompile_body(pos, end);
	stmt = if_else_stmt(stmt, body);

	(*pos_ptr) = end;
	return stmt;

      case FOR_RANGE:
	/* FOR_RANGE opcode follows two expressions. */
	/* End stored in second opcode argument is after the END opcode, so the
	   body ends at end - 1. */
	end = the_opcodes[pos + 1];
	var = varname(the_opcodes[pos + 2]);
	body = decompile_body(pos + 3, end - 2);
	(*pos_ptr) = end;
	return for_range_stmt(var, exprs->next->expr, exprs->expr, body);

      case FOR_LIST:
	/* FOR_LIST opcode follows two expressions; one is just a zero. */
	/* End stored in second opcode argument is after the END opcode, so the
	 * body ends at end - 1. */
	end = the_opcodes[pos + 1];
	var = varname(the_opcodes[pos + 2]);
	body = decompile_body(pos + 3, end - 2);
	(*pos_ptr) = end;
	return for_list_stmt(var, exprs->next->expr, body);

      case WHILE:
	/* WHILE statement follows one expression. */
	/* End stored in second opcode argument is after the END opcode, so the
	 * body ends at end - 1. */
	end = the_opcodes[pos + 1];
	body = decompile_body(pos + 3, end - 2);
	(*pos_ptr) = end;
	return while_stmt(exprs->expr, body);

      case SWITCH: {
	  Case_list *cases;

	  /* SWITCH statement follows one opcode. */
	  end = the_opcodes[pos + 1];
	  cases = decompile_cases(pos + 2, end);
	  (*pos_ptr) = end;
	  return switch_stmt(exprs->expr, cases);
      }

      case BREAK:
	/* BREAK opcode follows no expressions. */
	(*pos_ptr) = pos + 3;
	return break_stmt();

      case CONTINUE:
	/* CONTINUE opcode follows no expressions. */
	(*pos_ptr) = pos + 3;
	return continue_stmt();

      case RETURN:
	/* RETURN opcode follows no expressions. */
	(*pos_ptr) = pos + 1;
	return return_stmt();

      case RETURN_EXPR:
	/* RETURN_EXPR opcode follows one expression. */
	(*pos_ptr) = pos + 1;
	return return_expr_stmt(exprs->expr);

      case CATCH: {
	  Id_list *errors;

	  /* CATCH opcode follows no expressions. */
	  errors = make_error_id_list(the_opcodes[pos + 2]);
	  end = the_opcodes[pos + 1];
	  body = decompile_body(pos + 3, end - 2);
	  if (the_opcodes[end] == HANDLER_END)
	      stmt = NULL;
	  else
	      stmt = decompile_until(&end, HANDLER_END);
	  (*pos_ptr) = end + 1;
	  return catch_stmt(errors, body, stmt);
      }

      default:
	return NULL;
    }
}

static Stmt_list *decompile_stmt_list(Int start, Int end)
{
    Stmt_list *stmts = NULL;

    while (start < end)
	stmts = stmt_list(decompile_stmt(&start), stmts);
    return stmts;
}

static Stmt *decompile_until(Int *start, Int marker)
{
    Stmt_list *stmts = NULL;

    while (the_opcodes[*start] != marker)
	stmts = stmt_list(decompile_stmt(start), stmts);
    return body_from_stmt_list(stmts);
}

static Stmt *decompile_body(Int start, Int end)
{
    return body_from_stmt_list(decompile_stmt_list(start, end));
}

static Stmt *body_from_stmt_list(Stmt_list *stmts)
{
    if (!stmts)
	return noop_stmt();
    else if (stmts->next)
	return compound_stmt(stmts);
    else
	return stmts->stmt;
}

static Case_list *decompile_cases(Int start, Int end)
{
    Case_list *cases = NULL;

    while (start < end)
	cases = case_list(decompile_case(&start, end), cases);

    /* If the default case has no action, forget about it. */
    if (!cases->case_entry->stmts)
	cases = cases->next;

    return cases;
}

static Case_entry *decompile_case(Int *pos_ptr, Int switch_end)
{
    Expr_list *values;
    Stmt_list *stmts;
    Int end;

    values = decompile_case_values(pos_ptr, &end);
    if (!values) {
	/* It's a default statement; decompile up to the switch end. */
	stmts = decompile_stmt_list(*pos_ptr, switch_end);
	*pos_ptr = switch_end;
    } else {
	/* Decompile up to the END_CASE instruction at the end of the body. */
	stmts = decompile_stmt_list(*pos_ptr, end - 2);
	*pos_ptr = end;
    }
    return case_entry(values, stmts);
}

static Expr_list *decompile_case_values(Int *pos_ptr, Int *end_ret)
{
    Expr_list *exprs, *values = NULL;
    Expr *range;
    Int pos = *pos_ptr;

    if (the_opcodes[pos] == DEFAULT) {
	(*pos_ptr) = pos + 1;
	return NULL;
    }

    /* Loop until we hit the last case opcode for this case. */
    forever {
	/* Get any expressions preceding the case opcode. */
	exprs = decompile_expressions(&pos);

	switch (the_opcodes[pos]) {

	  case CASE_VALUE:
	    pos += 2;
	    values = expr_list(exprs->expr, values);
	    break;

	  case CASE_RANGE:
	    pos += 2;
	    range = range_expr(exprs->next->expr, exprs->expr);
	    values = expr_list(range, values);
	    break;

	  case LAST_CASE_VALUE:
	    *end_ret = the_opcodes[pos + 1];
	    *pos_ptr = pos + 2;
	    return expr_list(exprs->expr, values);

	  case LAST_CASE_RANGE:
	    *end_ret = the_opcodes[pos + 1];
	    *pos_ptr = pos + 2;
	    range = range_expr(exprs->next->expr, exprs->expr);
	    return expr_list(range, values);
	}
    }
}

static Id_list *make_error_id_list(Int which)
{
    Error_list *elist;
    Id_list *idl = NULL;
    Int i;

    /* A -1 indicates a 'catch all' expression; return a null ID list. */
    if (which == -1)
	return NULL;

    /* Retrieve the error list from the method by its index. */
    elist = &the_method->error_lists[which];

    /* Traverse the error list, building an id list from the id numbers.
     * Since the list gets flipped internally, this list should be read
     * forwards later on. */

    for (i = 0; i < elist->num_errors; i++)
	idl = id_list(ident_name(elist->error_ids[i]), idl);

    return idl;
}

/* Decompile an unbounded list of expressions. */
static Expr_list *decompile_expressions(Int *pos_ptr)
{
    return decompile_expressions_bounded(pos_ptr, -1);
}

static Expr *decompile_scatter (Int *pos_ptr)
{
    Int pos=*pos_ptr, end;
    Expr_list *args = NULL;
    char *s;
    Int is_splice=FALSE;

    while (1) {
	switch (the_opcodes[pos]) {
	case SET_LOCAL:
	    s = varname(the_opcodes[pos + 1]);
	    args = expr_list(var_expr(s), args);
	    pos += 2;
	    break;
	    
	case SET_OBJ_VAR:
	    s = ident_name(object_get_ident(the_object, the_opcodes[pos + 1]));
	    args = expr_list(var_expr(s), args);
	    pos += 2;
	    break;

	case OPTIONAL_ASSIGN: {
	    Expr_list *rhs;

	    end = the_opcodes[pos + 1];
	    pos += 2;
	    rhs = decompile_expressions_bounded(&pos, end);
	    pos = end + 1; /* skip OPTIONAL_END */
	    args = expr_list(opt_expr(rhs->expr->u.assign.lval->u.name,
				      rhs->expr->u.assign.value),
			     args);
	    break;
	}
	case SCATTER_START:
	    pos++;
	    args = expr_list(decompile_scatter(&pos), args);
	    break;

	case SPLICE:
	    pos++;
	    is_splice=TRUE;
	    break;

	case SCATTER_END:
	    if (is_splice)
		args->expr = splice_expr(args->expr);
	    *pos_ptr=pos+1;
	    return list_expr(args);
	}
    }
}


/* This function constructs the list of expressions that would result from
 * interpreting the opcodes starting at (*pos_ptr).  We stop at end, at a
 * statement token, or at a token which pops an argument list off the stack. */
static Expr_list *decompile_expressions_bounded(Int *pos_ptr, Int expr_end)
{
    Int pos = *pos_ptr, end;
    Expr_list *stack = NULL;
    char *s;

    while (expr_end == -1 || pos < expr_end) {
	switch (the_opcodes[pos]) {

	  case ZERO:
	    stack = expr_list(integer_expr(0), stack);
	    pos++;
	    break;

	  case ONE:
	    stack = expr_list(integer_expr(1), stack);
	    pos++;
	    break;

	  case INTEGER:
	    stack = expr_list(integer_expr(the_opcodes[pos + 1]), stack);
	    pos += 2;
	    break;

          case FLOAT:
            stack = expr_list(float_expr(*((Float*)(&the_opcodes[pos+1]))), stack);
            pos += 2;
            break;

	  case STRING:
	    s = string_chars(the_object->strings[the_opcodes[pos + 1]].str);
	    stack = expr_list(string_expr(s), stack);
	    pos += 2;
	    break;

	  case OBJNUM:
	    stack = expr_list(objnum_expr(the_opcodes[pos + 1]), stack);
	    pos += 2;
	    break;

	  case SYMBOL:
	    s = ident_name(object_get_ident(the_object, the_opcodes[pos + 1]));
	    stack = expr_list(symbol_expr(s), stack);
	    pos += 2;
	    break;

	  case T_ERROR:
	    s = ident_name(object_get_ident(the_object, the_opcodes[pos + 1]));
	    stack = expr_list(error_expr(s), stack);
	    pos += 2;
	    break;

	  case OBJNAME:
	    s = ident_name(object_get_ident(the_object, the_opcodes[pos + 1]));
	    stack = expr_list(objname_expr(s), stack);
	    pos += 2;
	    break;

	  case GET_LOCAL:
	    s = varname(the_opcodes[pos + 1]);
	    stack = expr_list(var_expr(s), stack);
	    pos += 2;
	    break;

	  case GET_OBJ_VAR:
	    s = ident_name(object_get_ident(the_object, the_opcodes[pos + 1]));
	    stack = expr_list(var_expr(s), stack);
	    pos += 2;
	    break;

          case P_INCREMENT:
          case P_DECREMENT:
          case INCREMENT:
          case DECREMENT:
            if (the_opcodes[pos + 1] == SET_OBJ_VAR)
	        s = ident_name(object_get_ident(the_object,the_opcodes[pos+2]));
            else
                s = varname(the_opcodes[pos+2]);
            stack->expr = indecr_expr(the_opcodes[pos], s);
            pos+=3;
	    break;

          case MULT_EQ:
          case DIV_EQ:
          case PLUS_EQ:
          case MINUS_EQ:
            /* ignore the current expr, it is just a GET_VAR */
            stack = stack->next;
            if (the_opcodes[pos + 1] == SET_OBJ_VAR)
	        s = ident_name(object_get_ident(the_object,the_opcodes[pos+2]));
            else
                s = varname(the_opcodes[pos+2]);
            stack->expr = doeq_expr(the_opcodes[pos], s, stack->expr);
            pos += 3;
            break;

	  case SET_LOCAL:
            /* SET_LOCAL opcode follows one expression. */
            s = varname(the_opcodes[pos + 1]);
            stack->expr = assign_expr(var_expr(s), stack->expr);
            pos += 2;
            break;

          case SET_OBJ_VAR:
            /* SET_OBJ_VAR opcode follows one expression. */
            s = ident_name(object_get_ident(the_object, the_opcodes[pos + 1]));
            stack->expr = assign_expr(var_expr(s), stack->expr);
            pos += 2;
            break;

	  case OPTIONAL_ASSIGN: {
	    /* This is optional assignment. It can't be in the scatter,
	       since it's handled from there. */
	    
	      Expr_list *rhs;

	      end = the_opcodes[pos + 1];
	      pos += 2;
	      rhs = decompile_expressions_bounded(&pos, end);
	      pos = end;
	      stack = expr_list(opt_expr(rhs->expr->u.assign.lval->u.name,
					 rhs->expr->u.assign.value),
				stack->next /*skip the get_var*/ );
	      break;
	  }

	  case SCATTER_START:
	      pos++;
	      stack->expr = assign_expr(decompile_scatter(&pos), stack->expr);
	      break;

	  case START_ARGS: {
	      Expr_list *args;

	      pos++;
	      args = decompile_expressions(&pos);
	      switch(the_opcodes[pos]) {

		case PASS:
		  stack = expr_list(pass_expr(args), stack);
		  pos++;
		  break;

		case CALL_METHOD:
		  s = ident_name(object_get_ident(the_object,
						  the_opcodes[pos + 1]));
		  stack->expr = message_expr(stack->expr, s, args);
		  pos += 2;
		  break;

		case EXPR_CALL_METHOD:
		  stack->next->expr = expr_message_expr(stack->next->expr,
							stack->expr, args);
		  stack = stack->next;
		  pos++;
		  break;

		case LIST:
		  stack = expr_list(list_expr(args), stack);
		  pos++;
		  break;

		case DICT:
		  stack = expr_list(dict_expr(args), stack);
		  pos++;
		  break;

		case BUFFER:
		  stack = expr_list(buffer_expr(args), stack);
		  pos++;
		  break;

		default:
		  s = op_table[the_opcodes[pos]].name;
		  if (!s)
		      panic("Invalid expression opcode.");
		  stack = expr_list(function_call_expr(s, args), stack);
		  pos++;
		  break;
	      }
	      break;
	  }

	  case FROB:
	    stack->next->expr = frob_expr(stack->next->expr, stack->expr, NULL);
	    stack = stack->next;
	    pos++;
	    break;

	  case OP_HANDLED_FROB:
	    stack->next->next->expr = frob_expr(stack->next->next->expr,
						stack->next->expr, stack->expr);
	    stack = stack->next->next;
	    pos++;
	    break;

	  case INDEX:
	    stack->next->expr = index_expr(stack->next->expr, stack->expr);
	    stack = stack->next;
	    pos++;
	    break;

	  case AND: {
	      Expr_list *rhs;

	      end = the_opcodes[pos + 1];
	      pos += 2;
	      rhs = decompile_expressions_bounded(&pos, end);
	      stack->expr = and_expr(stack->expr, rhs->expr);
	      pos = end;
	      break;
	  }

	  case OR: {
	      Expr_list *rhs;

	      end = the_opcodes[pos + 1];
	      pos += 2;
	      rhs = decompile_expressions_bounded(&pos, end);
	      stack->expr = or_expr(stack->expr, rhs->expr);
	      pos = end;
	      break;
	  }

	  case OP_MAPHASH:
	  case OP_FILTER:
	  case OP_FIND:
  	  case OP_MAP: {
	      Expr_list *job;
	      Int etype=the_opcodes[pos];

	      end = the_opcodes[pos + 1];
	      s = varname(the_opcodes[pos + 2]);
	      pos+=3;
	      job = decompile_expressions_bounded(&pos, end - 2);
	      pos = end;
	      /* delete the three ZEROs */
	      stack = stack->next->next->next;
	      stack->expr = map_expr(stack->expr, s, job->expr,etype);
	      break;
	  }

	  case OP_MAPHASH_RANGE:
	  case OP_FILTER_RANGE:
	  case OP_FIND_RANGE:
  	  case OP_MAP_RANGE: {
	      Expr_list *job;
	      Expr *start_expr, *end_expr;
	      Int etype=the_opcodes[pos];

	      end = the_opcodes[pos + 1];
	      s = varname(the_opcodes[pos + 2]);
	      pos+=3;
	      job = decompile_expressions_bounded(&pos, end - 2);
	      pos = end;
	      /* delete the ZEROs */
	      stack = stack->next->next;
	      start_expr = stack->next->expr;
	      end_expr = stack->expr;
	      stack = stack->next;
	      stack->expr = map_range_expr(start_expr, end_expr, s, job->expr,etype);
	      break;
	  }

	  case CONDITIONAL: {
	      Expr_list *true, *false;

	      /* The end stored in opcodes[pos + 1] is after the ELSE
	       * instruction. */
	      /* Get true expression. */
	      end = the_opcodes[pos + 1];
	      pos += 2;
	      true = decompile_expressions_bounded(&pos, end - 2);

	      /* Get false expression. */
	      pos = end;
	      end = the_opcodes[pos - 1];
	      false = decompile_expressions_bounded(&pos, end);

	      stack->expr = cond_expr(stack->expr, true->expr, false->expr);
	      pos = end;
	      break;
	  }

	  case CRITICAL:
	    /* Ignore this, and look for CRITICAL_END instead. */
	    pos += 2;
	    break;

	  case CRITICAL_END:
	    stack->expr = critical_expr(stack->expr);
	    pos++;
	    break;

	  case PROPAGATE:
	    /* Ignore this, and look for PROPAGATE_END instead. */
	    pos += 2;
	    break;

	  case PROPAGATE_END:
	    stack->expr = propagate_expr(stack->expr);
	    pos++;
	    break;

	  case '!':
	  case NEG:
	    stack->expr = unary_expr(the_opcodes[pos], stack->expr);
	    pos++;
	    break;

	  case '*':
	  case '/':
	  case '%':
	  case '+':
	  case '-':
	  case EQ:
	  case NE:
	  case '>':
	  case GE:
	  case '<':
	  case LE:
	  case OP_IN:
	    stack->next->expr = binary_expr(the_opcodes[pos],
					    stack->next->expr, stack->expr);
	    stack = stack->next;
	    pos++;
	    break;

	  case SPLICE_ADD: {
	      Expr_list **elistp = &stack->expr->u.args;

	      /* Find the end of the list expression's argument list. */
	      while (*elistp)
		  elistp = &(*elistp)->next;

	      /* Append the spliced expression to the end. */
	      *elistp = expr_list(splice_expr(stack->next->expr), NULL);

	      /* Shift the new list expression back one on the stack. */
	      stack->next->expr = stack->expr;
	      stack = stack->next;
	      pos++;
	      break;
	  }

	  case SPLICE:
	    stack->expr = splice_expr(stack->expr);
	    pos++;
	    break;

	  default:
	    /* We've hit a statement token, or a token that takes an argument
	     * list.  Stop. */
	    *pos_ptr = pos;
	    return stack;
	}
    }

    /* We hit the end.  Stop. */
    *pos_ptr = pos;
    return stack;
}

/* Write a statement list onto the output list, backwards. */
static cList *unparse_stmt_list(cList *output, Stmt_list *stmts, Int indent)
{
    Stmt *last;

    if (stmts) {
	last = stmts->next ? stmts->next->stmt : NULL;
	output = unparse_stmt_list(output, stmts->next, indent);
	return unparse_stmt(output, stmts->stmt, indent, last);
    } else {
	return output;
    }
}

static cList *unparse_stmt(cList *output, Stmt *stmt, Int indent, Stmt *last)
{
    cStr *str;

    switch (stmt->type) {

      /* This switch doesn't include no-op statements or RETURN statements,
       * since we never see them here. */

      case NOOP:
        str = string_of_char(' ', indent);
        str = string_addc(str, ';');
        return add_and_discard_string(output, str);

      case COMMENT:
	/* Add a blank line if there is a previous line and it is not a
	 * comment. */
	if (last && last->type != COMMENT)
	    output = add_and_discard_string(output, string_new(0));
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "//", 2);
	str = string_add_chars(str, stmt->u.comment, strlen(stmt->u.comment));
	return add_and_discard_string(output, str);

      case EXPR:
	str = string_of_char(' ', indent);
	str = unparse_expr(str, stmt->u.expr, NOPAREN_ASSIGN);
	str = string_addc(str, ';');
	return add_and_discard_string(output, str);

      case COMPOUND:
	/* The calling routine handles the positioning of the braces. */
	return unparse_stmt_list(output, stmt->u.stmt_list, indent);

      case IF:
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "if (", 4);
	str = unparse_expr(str, stmt->u.if_.cond, PAREN_ASSIGN);
	str = string_addc(str, ')');
	return unparse_body(output, stmt->u.if_.true, str, indent);

      case IF_ELSE: {
	  Int complex;

	  /* This is the most complicated unparsing job, because we need to
	   * decide to format both the if-true and if-false statements, and we
	   * also want to correctly format chains of "if ... else if ... ". */

	  /* Determine if either of the statements is complex (will take more
	   * than one line).  Do this just once at the beginning. */
	  complex = is_complex_if_else_stmt(stmt);

	  /* Start with a string of spaces one less than the indent. */
	  str = string_of_char(' ', indent);

	  /* Execute this for stmt as well as any else-ifs. */
	  while (stmt->type == IF_ELSE) {
	      str = string_add_chars(str, "if (", 4);
	      str = unparse_expr(str, stmt->u.if_.cond, PAREN_ASSIGN);
	      str = string_addc(str, ')');

	      if (stmt->u.if_.cond->type == NOOP) {
		  str = string_addc(str, ';');
		  output = add_and_discard_string(output, str);
		  str = string_of_char(' ', indent);
	      } else if (complex) {
		  str = string_add_chars(str, " {", 2);
		  output = add_and_discard_string(output, str);
		  output = unparse_stmt(output, stmt->u.if_.true,
					indent + the_increment, NULL);
		  str = string_of_char(' ', indent);
		  str = string_add_chars(str, "} ", 2);
	      } else {
		  output = add_and_discard_string(output, str);
		  output = unparse_stmt(output, stmt->u.if_.true,
					indent + the_increment, NULL);
		  str = string_of_char(' ', indent);
	      }
	      
	      str = string_add_chars(str, "else ", 5);

	      /* Set stmt to stmt->u.if_.false, so that we will continue the
	       * loop if the false statement is an if statement. */
	      stmt = stmt->u.if_.false;
	  }

	  if (stmt->type == IF) {
	      str = string_add_chars(str, "if (", 4);
	      str = unparse_expr(str, stmt->u.if_.cond, PAREN_ASSIGN);
	      str = string_add_chars(str, ") ", 2);
	      stmt = stmt->u.if_.true;
	  }

	  /* Now unparse the final statement, which is in stmt. */
	  if (stmt->type == NOOP) {
	      /* Replace the trailing space with a semicolon. */
	      str = string_truncate(str, string_length(str) - 1);
	      str = string_addc(str, ';');
	      return add_and_discard_string(output, str);
	  } else if (complex) {
	      str = string_addc(str, '{');
	      output = add_and_discard_string(output, str);
	      output = unparse_stmt(output, stmt, indent + the_increment,
				    NULL);
	      str = string_of_char(' ', indent);
	      str = string_addc(str, '}');
	      return add_and_discard_string(output, str);
	  } else {
	      /* Just eliminate the trailing space. */
	      str = string_truncate(str, string_length(str) - 1);
	      output = add_and_discard_string(output, str);
	      return unparse_stmt(output, stmt, indent + the_increment, NULL);
	  }
      }

      case FOR_RANGE: {
	  char *s;

	  s = stmt->u.for_range.var;
	  str = string_of_char(' ', indent);
	  str = string_add_chars(str, "for ", 4);
	  str = string_add_chars(str, s, strlen(s));
	  str = string_add_chars(str, " in [", 5);
	  str = unparse_expr(str, stmt->u.for_range.lower, PAREN_ASSIGN);
	  str = string_add_chars(str, " .. ", 4);
	  str = unparse_expr(str, stmt->u.for_range.upper, PAREN_ASSIGN);
	  str = string_addc(str, ']');
	  return unparse_body(output, stmt->u.for_range.body, str, indent);
      }

      case FOR_LIST: {
	  char *s;

	  s = stmt->u.for_list.var;
	  str = string_of_char(' ', indent);
	  str = string_add_chars(str, "for ", 4);
	  str = string_add_chars(str, s, strlen(s));
	  str = string_add_chars(str, " in (", 5);
	  str = unparse_expr(str, stmt->u.for_list.list, PAREN_ASSIGN);
	  str = string_addc(str, ')');
	  return unparse_body(output, stmt->u.for_list.body, str, indent);
      }

      case WHILE:
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "while (", 7);
	str = unparse_expr(str, stmt->u.while_.cond, PAREN_ASSIGN);
	str = string_addc(str, ')');
	return unparse_body(output, stmt->u.while_.body, str, indent);

      case SWITCH:
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "switch (", 8);
	str = unparse_expr(str, stmt->u.switch_.expr, PAREN_ASSIGN);
	str = string_add_chars(str, ") {", 3);
	output = add_and_discard_string(output, str);
	output = unparse_cases(output, stmt->u.switch_.cases,
			       indent + the_increment);
	str = string_of_char(' ', indent);
	str = string_addc(str, '}');
	return add_and_discard_string(output, str);

      case BREAK:
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "break;", 6);
	return add_and_discard_string(output, str);

      case CONTINUE:
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "continue;", 9);
	return add_and_discard_string(output, str);

      case RETURN:
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "return;", 7);
	return add_and_discard_string(output, str);
	
      case RETURN_EXPR:
	str = string_of_char(' ', indent);
	str = string_add_chars(str, "return ", 7);
	str = unparse_expr(str, stmt->u.expr, PAREN_ASSIGN);
	str = string_addc(str, ';');
	return add_and_discard_string(output, str);

      case CATCH: {
	  Id_list *errors;
          Int complex = 0;

          if ((stmt->u.ccatch.body != NULL &&
               is_complex_type(stmt->u.ccatch.body->type)) ||
              (stmt->u.ccatch.handler != NULL &&
               is_complex_type(stmt->u.ccatch.handler->type)))
              complex = 1;

	  str = string_of_char(' ', indent);
	  str = string_add_chars(str, "catch ", 6);
	  errors = stmt->u.ccatch.errors;
	  if (errors) {
	      for (; errors; errors = errors->next) {
		  str = string_addc(str, '~');
		  str = string_add_chars(str, errors->ident, strlen(errors->ident));
		  if (errors->next)
		      str = string_add_chars(str, ", ", 2);
	      }
	  } else {
	     str = string_add_chars(str, "any", 3);
	  }
          if (complex)
              str = string_add_chars(str, " {", 2);
	  output = add_and_discard_string(output, str);
	  output = unparse_stmt(output, stmt->u.ccatch.body,
				indent + the_increment, NULL);
	  if (stmt->u.ccatch.handler) {
	      str = string_of_char(' ', indent);
              if (complex)
                  str = string_add_chars(str, "} with {", 8);
              else
                  str = string_add_chars(str, "with", 4);
	      output = add_and_discard_string(output, str);
	      output = unparse_stmt(output, stmt->u.ccatch.handler,
				    indent + the_increment, NULL);
	  }
          if (complex) {
              str = string_of_char(' ', indent);
              str = string_addc(str, '}');
              output = add_and_discard_string(output, str);
          }
          return output;
      }

      default:
	return output;
    }
}

/* Returns nonzero if the if-else statment stmt is complex--that is, if we
 * should display its true and false clauses in braces.  If the false clause
 * is an IF_ELSE or an IF statement, then we will be displaying the false
 * clause's if statment on the same line as our own else statement, so we must
 * check to see if the false clause itself is complex.  Apart from that, it's
 * just a matter of testing if either the true or false clause is complex. */
static Int is_complex_if_else_stmt(Stmt *stmt)
{
    if (is_complex_type(stmt->u.if_.true->type))
	return 1;
    else if (stmt->u.if_.false->type == IF_ELSE)
	return is_complex_if_else_stmt(stmt->u.if_.false);
    else if (stmt->u.if_.false->type == IF)
	return is_complex_type(stmt->u.if_.false->u.if_.true->type);
    else
	return is_complex_type(stmt->u.if_.false->type);
}

static Int is_complex_type(Int type)
{
    return FULL_BRACES() ||
           (type != NOOP && type != EXPR && type != ASSIGN && type != BREAK &&
	    type != CONTINUE && type != RETURN && type != RETURN_EXPR);
}

static cList *unparse_body(cList *output, Stmt *body, cStr *str, Int indent)
{
    if (is_complex_type(body->type)) {
	str = string_add_chars(str, " {", 2);
	output = add_and_discard_string(output, str);
	output = unparse_stmt(output, body, indent + the_increment, NULL);
	str = string_of_char(' ', indent);
	str = string_addc(str, '}');
	return add_and_discard_string(output, str);
    } else {
	output = add_and_discard_string(output, str);
	return unparse_stmt(output, body, indent + the_increment, NULL);
    }
}

static cList *unparse_cases(cList *output, Case_list *cases, Int indent)
{
    if (cases) {
	output = unparse_cases(output, cases->next, indent);
	output = unparse_case(output, cases->case_entry, indent);
    }
    return output;
}

static cList *unparse_case(cList *output, Case_entry *case_entry, Int indent)
{
    cStr *str;

    str = string_of_char(' ', indent);
    if (!case_entry->values) {
	str = string_add_chars(str, "default:", 8);
    } else {
	str = string_add_chars(str, "case ", 5);
	str = unparse_args(str, case_entry->values);
	str = string_addc(str, ':');
    }
    output = add_and_discard_string(output, str);
    return unparse_stmt_list(output, case_entry->stmts,
			     indent + the_increment);
}

static cStr *unparse_expr(cStr *str, Expr *expr, Int paren) {
    char *s;

    switch (expr->type) {

      case INTEGER: {
	  Number_buf nbuf;

	  s = long_to_ascii(expr->u.num, nbuf);
	  return string_add_chars(str, s, strlen(s));
      }

      case FLOAT: {
          Number_buf nbuf;

	  s=float_to_ascii(expr->u.fnum,nbuf);
	  return string_add_chars(str, s, strlen(s));
      }

      case STRING:
	return string_add_unparsed(str, expr->u.str, strlen(expr->u.str));

      case OBJNUM: {
	  Number_buf nbuf;

	  str = string_addc(str, '#');
	  s = long_to_ascii(expr->u.objnum, nbuf);
	  return string_add_chars(str, s, strlen(s));
      }

      case SYMBOL:
	s = expr->u.symbol;
	str = string_addc(str, '\'');
	if (is_valid_ident(expr->u.symbol))
	    return string_add_chars(str, s, strlen(s));
	else
	    return string_add_unparsed(str, s, strlen(s));

      case T_ERROR:
	s = expr->u.error;
	str = string_addc(str, '~');
	if (is_valid_ident(expr->u.error))
	    return string_add_chars(str, s, strlen(s));
	else
	    return string_add_unparsed(str, s, strlen(s));

      case OBJNAME:
	s = expr->u.name;
	str = string_addc(str, '$');
	if (is_valid_ident(expr->u.name))
	    return string_add_chars(str, s, strlen(s));
	else
	    return string_add_unparsed(str, s, strlen(s));

      case VAR:
	return string_add_chars(str, expr->u.name, strlen(expr->u.name));

      case ASSIGN:
        if (paren)
            str = string_addc(str, '(');
	str = unparse_expr(str, expr->u.assign.lval, PAREN_ASSIGN);
	str = string_add_chars(str, " = ", 3);
	str = unparse_expr(str, expr->u.assign.value, PAREN_ASSIGN);
	if (paren)
	    str = string_addc(str, ')');
        return str;

      case OPTIONAL_ASSIGN:
        s = expr->u.optassign.var;
        if (paren)
            str = string_addc(str, '(');
	str = string_add_chars(str, s, strlen(s));
	str = string_add_chars(str, " ?= ", 4);
	str = unparse_expr(str, expr->u.optassign.value, PAREN_ASSIGN);
	if (paren)
	    str = string_addc(str, ')');
        return str;

      case INDECR:
        s = expr->u.doeq.var;

        /* redundant code */
        switch (expr->u.doeq.opcode) {
            case P_INCREMENT:
                str = string_add_chars(str, "++", 2);
                str = string_add_chars(str, s, strlen(s));
                break;
            case P_DECREMENT:
                str = string_add_chars(str, "--", 2);
                str = string_add_chars(str, s, strlen(s));
                break;
            case INCREMENT:
                str = string_add_chars(str, s, strlen(s));
                str = string_add_chars(str, "++", 2);
                break;
            case DECREMENT:
                str = string_add_chars(str, s, strlen(s));
                str = string_add_chars(str, "--", 2);
                break;
        }
        return str;

      case DOEQ:
        s = expr->u.doeq.var;
        if (paren)
            str = string_addc(str, '(');
        str = string_add_chars(str, s, strlen(s));
        switch(expr->u.doeq.opcode) {
            case MULT_EQ:
                str = string_add_chars(str, " *= ", 4); break;
            case DIV_EQ:
                str = string_add_chars(str, " /= ", 4); break;
            case PLUS_EQ:
                str = string_add_chars(str, " += ", 4); break;
            case MINUS_EQ:
                str = string_add_chars(str, " -= ", 4); break;
        }
        str = unparse_expr(str, expr->u.doeq.value, PAREN_ASSIGN);
        if (paren)
            str = string_addc(str, ')');
        return str;
      
      case FUNCTION_CALL:
	s = expr->u.function.name;
	str = string_add_chars(str, s, strlen(s));
	str = string_addc(str, '(');
	str = unparse_args(str, expr->u.function.args);
	return string_addc(str, ')');

      case PASS:
	str = string_add_chars(str, "pass(", 5);
	str = unparse_args(str, expr->u.args);
	return string_addc(str, ')');

      case CALL_METHOD:
	s = expr->u.message.name;

	/* Only include target if it's not this(). */
        if (expr->u.message.to->type==INTEGER)
            str = string_addc (str,'(');
	if (!is_this(expr->u.message.to))
	    str = unparse_expr_prec(str, expr->u.message.to, CALL_METHOD, 0);
        if (expr->u.message.to->type==INTEGER)
            str = string_addc (str,')');

	str = string_addc(str, '.');
	str = string_add_chars(str, s, strlen(s));
	str = string_addc(str, '(');
	str = unparse_args(str, expr->u.message.args);
	str = string_addc(str, ')');
	return str;

      case EXPR_CALL_METHOD:
	/* Only include target if it's not this(). */

        if (expr->u.message.to->type==INTEGER)
            str = string_addc (str,'(');
	if (!is_this(expr->u.expr_message.to))
	    str = unparse_expr_prec(str, expr->u.message.to, CALL_METHOD, 0);
        if (expr->u.message.to->type==INTEGER)
            str = string_addc (str,')');

	str = string_add_chars(str, ".(", 2);
	str = unparse_expr(str, expr->u.expr_message.message, PAREN_ASSIGN);
	str = string_addc(str, ')');
	str = string_addc(str, '(');
	str = unparse_args(str, expr->u.expr_message.args);
	return string_addc(str, ')');

      case LIST:
	str = string_addc(str, '[');
	str = unparse_args(str, expr->u.args);
	return string_addc(str, ']');

      case DICT:
	str = string_add_chars(str, "#[", 2);
	str = unparse_args(str, expr->u.args);
	return string_addc(str, ']');

      case BUFFER:
	str = string_add_chars(str, "`[", 2);
	str = unparse_args(str, expr->u.args);
	return string_addc(str, ']');

      case FROB:
        /* $#$NOTE: frobs shouldn't have to have ()'s around them */
	str = string_add_chars(str, "(<", 2);
	str = unparse_complex_expr(str, expr->u.frob.cclass, FROB, 0);
	str = string_add_chars(str, ", ", 2);
	str = unparse_complex_expr(str, expr->u.frob.rep, FROB, 0);
	if (expr->u.frob.handler) {
	    str = string_add_chars(str, ", ", 2);
	    str = unparse_complex_expr(str, expr->u.frob.handler, FROB, 0);
	}
	return string_add_chars(str, ">)", 2);

      case INDEX:
	str = unparse_expr_prec(str, expr->u.index.list, INDEX, 0);
	str = string_addc(str, '[');
	str = unparse_expr(str, expr->u.index.offset, PAREN_ASSIGN);
	return string_addc(str, ']');

      case UNARY: {
	  Int opcode = expr->u.unary.opcode;

	  str = string_addc(str, (opcode == NEG) ? '-' : opcode);
	  return unparse_expr_prec(str, expr->u.unary.expr, opcode, 0);
      }

      case BINARY: {
	  Int opcode = expr->u.binary.opcode;

	  s = binary_token(opcode);
	  str = unparse_expr_prec(str, expr->u.binary.left, opcode, 0);
	  str = string_addc(str, ' ');
	  str = string_add_chars(str, s, strlen(s));
	  str = string_addc(str, ' ');
	  return unparse_expr_prec(str, expr->u.binary.right, opcode, 1);
      }

      case AND:
	str = unparse_expr_prec(str, expr->u.and.left, AND, 1);
	str = string_add_chars(str, " && ", 4);
	return unparse_expr_prec(str, expr->u.and.right, AND, 0);

      case OR:
	str = unparse_expr_prec(str, expr->u.or.left, OR, 1);
	str = string_add_chars(str, " || ", 4);
	return unparse_expr_prec(str, expr->u.or.right, OR, 0);

      case OP_MAPHASH:
      case OP_FILTER:
      case OP_FIND:
      case OP_MAP: {
	char *s1, *s2;

	switch (expr->type) {
	  case OP_MAP:
	    s1="map ";
	    s2=" to ";
	    break;
	  case OP_FIND:
	    s1="find ";
	    s2=" where ";
	    break;
	  case OP_FILTER:
	    s1="filter ";
	    s2=" where ";
	    break;
	default:
	    s1="hash ";
	    s2=" to ";
        }
	s = expr->u.map.var;
	str = string_add_chars(str, s1, strlen(s1));
	str = string_add_chars(str, s, strlen(s));
	str = string_add_chars(str, " in ",4);
        str = string_addc(str, '(');
	str = unparse_expr(str, expr->u.map.src, PAREN_ASSIGN);
        str = string_addc(str, ')');
	str = string_add_chars(str, s2, strlen(s2));
        str = string_addc(str, '(');
	str = unparse_expr(str, expr->u.map.job, PAREN_ASSIGN);
        str = string_addc(str, ')');
	return str;
      }

      case OP_MAPHASH_RANGE:
      case OP_FILTER_RANGE:
      case OP_FIND_RANGE:
      case OP_MAP_RANGE: {
	char *s1, *s2;

	switch (expr->type) {
	  case OP_MAP_RANGE:
	    s1="map ";
	    s2=" to ";
	    break;
	  case OP_FIND_RANGE:
	    s1="find ";
	    s2=" where ";
	    break;
	  case OP_FILTER_RANGE:
	    s1="filter ";
	    s2=" where ";
	    break;
	  default:
	    s1="hash ";
	    s2=" to ";
        }
        s = expr->u.maprange.var;
        str = string_add_chars(str, s1, strlen(s1));
        str = string_add_chars(str, s, strlen(s));
        str = string_add_chars(str, " in ",4);
        str = string_addc(str, '['); 
        str = unparse_expr(str, expr->u.maprange.start, PAREN_ASSIGN);
	str = string_add_chars(str, " .. ",4);
        str = unparse_expr(str, expr->u.maprange.end, PAREN_ASSIGN);
        str = string_addc(str, ']');
        str = string_add_chars(str, s2, strlen(s2));
        str = string_addc(str, '('); 
        str = unparse_expr(str, expr->u.maprange.job, PAREN_ASSIGN);
        str = string_addc(str, ')');
        return str;
      }

      case CONDITIONAL:
	str = unparse_expr_prec(str, expr->u.cond.cond, CONDITIONAL, 1);
	str = string_add_chars(str, " ? ", 3);
	str = unparse_expr_prec(str, expr->u.cond.true, CONDITIONAL, 1);
	str = string_add_chars(str, " : ", 3);
	str = unparse_expr_prec(str, expr->u.cond.false, CONDITIONAL, 1);
	return str;

      case CRITICAL:
	str = string_add_chars(str, "(| ", 3);
	str = unparse_expr(str, expr->u.expr, PAREN_ASSIGN);
	return string_add_chars(str, " |)", 3);

      case PROPAGATE:
	str = string_add_chars(str, "(> ", 3);
	str = unparse_expr(str, expr->u.expr, PAREN_ASSIGN);
	return string_add_chars(str, " <)", 3);

      case SPLICE:
	str = string_addc(str, '@');
	if (expr->u.expr->type == BINARY) {
	    /* Add parens around binary expressions for readability. */
	    str = string_addc(str, '(');
	    str = unparse_expr(str, expr->u.expr, PAREN_ASSIGN);
	    return string_addc(str, ')');
	} else {
	    return unparse_expr(str, expr->u.expr, PAREN_ASSIGN);
	}

      case RANGE:
	str = unparse_expr(str, expr->u.range.lower, PAREN_ASSIGN);
	str = string_add_chars(str, " .. ", 4);
	return unparse_expr(str, expr->u.range.upper, PAREN_ASSIGN);

      default:
	return str;
    }
}

static Int is_this(Expr *expr)
{
    return (expr->type == FUNCTION_CALL && !expr->u.function.args &&
	    strcmp(expr->u.function.name, "this") == 0);
}

/* Unparse an argument list, backwards.  The double check on args and
 * args->next is not redundant, since an argument list may be empty. */
static cStr *unparse_args(cStr *str, Expr_list *args)
{
    if (args) {
	if (args->next) {
	    str = unparse_args(str, args->next);
	    str = string_add_chars(str, ", ", 2);
	}
	str = unparse_expr(str, args->expr, PAREN_ASSIGN);
    }
    return str;
}

/* Unparse an expression, inserting parentheses around it if the caller
 * precedence is greater than the expression's precedence level.  If we
 * should put parentheses around the expression if it is at the same
 * precedence level because of assocation (e.g. a / (b * c)) then assoc
 * should be 1. */
static cStr *unparse_expr_prec(cStr *str, Expr *expr, Int caller_type,
				 Int assoc)
{
    Int caller_prec = prec_level(caller_type), type, prec;

    type = (expr->type == BINARY) ? expr->u.binary.opcode :
	(expr->type == UNARY) ? expr->u.unary.opcode : expr->type;
    prec = prec_level(type);

    if (prec >= 0 && (FULL_PARENS() || caller_prec + assoc > prec)) {
	str = string_addc(str, '(');
	str = unparse_expr(str, expr, PAREN_ASSIGN);
	return string_addc(str, ')');
    } else {
	return unparse_expr(str, expr, PAREN_ASSIGN);
    }
}

static cStr *unparse_complex_expr(cStr *str, Expr *expr, Int caller_type,
				  Int assoc)
{
    if (expr->type < DATA_END ||
        expr->type == FUNCTION_CALL ||
        expr->type == VAR) {
	return unparse_expr(str, expr, PAREN_ASSIGN);
    } else {
	str = string_addc(str, '(');
	str = unparse_expr(str, expr, PAREN_ASSIGN);
	return string_addc(str, ')');
    }
}

static Int prec_level(Int opcode)
{
    Int i;

    for (i = 0; i < PREC_SIZE; i++) {
	if (precedences[i].opcode == opcode)
	    return precedences[i].level;
    }

    /* Atomic expressions won't be in the table. */
    return -1;
}

static char *binary_token(Int opcode)
{
    Int i;

    for (i = 0; i < TOKEN_SIZE; i++) {
	if (binary_tokens[i].opcode == opcode)
	    return binary_tokens[i].token;
    }
    return "??";
}

static cList *add_and_discard_string(cList *output, cStr *str)
{
    cData d;

    d.type = STRING;
    d.u.str = str;
    output = list_add(output, &d);
    string_discard(str);
    return output;
}

/* Get the variable name for an index. */
static char *varname(Int ind)
{
    Long id;

    if (ind < the_method->num_args) {
	ind = the_method->num_args - ind - 1;
	id = object_get_ident(the_object, the_method->argnames[ind]);
	return ident_name(id);
    }
    ind -= the_method->num_args;

    if (the_method->rest != -1) {
	if (ind == 0) {
	    id = object_get_ident(the_object, the_method->rest);
	    return ident_name(id);
	}
	ind--;
    }

    id = object_get_ident(the_object, the_method->varnames[ind]);
    return ident_name(id);
}

