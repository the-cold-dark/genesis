/*
// Full copyright information is available in the file ../doc/CREDITS
*/

/*
// ------------------------------------------------------------
//
// C Declarations
//
*/

%{

#define _grammar_y_

#include "defs.h"

#include <stdarg.h>
#include "cdc_pcode.h"
#include "util.h"

int yyparse(void);
static void yyerror(char *s);

static Prog *prog;
static cList *errors;

extern Pile *compiler_pile;	/* We free this pile after compilation. */

%}

/*
// ------------------------------------------------------------
//
// yacc Declarations
//
*/

%union {
	Long			 num;
	Float			 fnum;
	char			*s;
	struct arguments	*args;
	struct stmt		*stmt;
	struct expr		*expr;
	struct id_list		*id_list;
	struct stmt_list	*stmt_list;
	struct expr_list	*expr_list;
	struct case_list	*case_list;
	struct case_entry	*case_entry;
};

%type	<num>		ovdecl
%type	<args>		argdecl
%type	<stmt>		stmt if
%type	<stmt_list>	compound stmts stmtlist
%type	<case_entry>	case_ent
%type	<case_list>	caselist cases
%type	<expr>		expr sexpr rexpr handler
%type	<expr_list>	args arglist cvals
%type	<s>		for map find filter hash
%type	<id_list>	vars idlist errors errlist

/* The following tokens are terminals for the parser. */

%token  FIRST_TOKEN

/* data */
%token	<num>	INTEGER OBJNUM
%token  <fnum>  FLOAT
%token	<s>	STRING SYMBOL OBJNAME T_ERROR
%token          LIST DICT BUFFER FROB

%token  DATA_END

/* not data */
%token  <s>     COMMENT IDENT
%token		DISALLOW_OVERRIDES ARG VAR
%token		IF FOR OP_IN UPTO WHILE SWITCH CASE DEFAULT
%token		BREAK CONTINUE RETURN
%token		CATCH ANY HANDLER
%token		FORK
%token		PASS CRITLEFT CRITRIGHT PROPLEFT PROPRIGHT

%right	OP_ASSIGN
%right	MINUS_EQ DIV_EQ MULT_EQ PLUS_EQ OPTIONAL_ASSIGN
%left	TO
%right	OP_COND_IF ':' OP_COND_OTHER_ELSE
%right	OR
%right	AND
%left	OP_IN
%left	EQ NE '>' GE '<' LE
%left	'+' '-'
%left	'*' '/' '%'
%left	'!'
%left	'[' ']' START_DICT START_BUFFER
%right	P_INCREMENT INCREMENT P_DECREMENT DECREMENT
%left	'.'

/* Declarations to shut up shift/reduce conflicts. */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%nonassoc LOWER_THAN_WITH
%nonassoc WITH

/* Mapping expressions */
%token OP_MAP WHERE OP_FILTER OP_MAPHASH OP_FIND

/* The parser does not use the following tokens.  I define them here so
 * that I can use them, along with the above tokens, as statement and
 * expression types for the code generator, and as opcodes for the
 * interpreter. */

%token NOOP EXPR COMPOUND ASSIGN IF_ELSE FOR_RANGE FOR_LIST END RETURN_EXPR
%token DOEQ INDECR
%token CASE_VALUE CASE_RANGE LAST_CASE_VALUE LAST_CASE_RANGE END_CASE RANGE
%token FUNCTION_CALL CALL_METHOD EXPR_CALL_METHOD LIST DICT BUFFER FROB INDEX UNARY
%token BINARY CONDITIONAL SPLICE NEG SPLICE_ADD POP START_ARGS ZERO ONE
%token SET_LOCAL SET_OBJ_VAR GET_LOCAL GET_OBJ_VAR CATCH_END HANDLER_END
%token CRITICAL CRITICAL_END PROPAGATE PROPAGATE_END JUMP
%token OPTIONAL_END SCATTER_START SCATTER_END

%token OP_MAP_RANGE OP_MAPHASH_RANGE OP_FILTER_RANGE OP_FIND_RANGE 

%token FUNCTION_START
%token F_DEBUG_CALLERS F_CALL_TRACE
%token F_TYPE F_CLASS F_TOINT F_TOFLOAT F_TOSTR F_TOLITERAL F_FROMLITERAL
%token F_FROB_CLASS F_TOOBJNUM F_TOSYM F_TOERR F_VALID F_STRFMT F_STRLEN F_STRIDX
%token F_SUBSTR F_EXPLODE F_STRSED F_STRSUB F_PAD F_MATCH_BEGIN
%token F_MATCH_TEMPLATE F_STRGRAFT F_LISTGRAFT F_BUFGRAFT
%token F_MATCH_PATTERN F_MATCH_REGEXP F_REGEXP F_SPLIT F_CRYPT F_UPPERCASE
%token F_LOWERCASE F_STRCMP F_LISTLEN F_SUBLIST F_INSERT F_JOIN F_REPLACE
%token F_LISTIDX F_DELETE F_SETADD F_SETREMOVE F_UNION F_MATCH_CRYPTED
%token F_DICT_KEYS F_DICT_VALUES F_DICT_ADD F_DICT_DEL F_DICT_UNION
%token F_DICT_CONTAINS F_BUFLEN F_BUFIDX F_BUF_REPLACE
%token F_BUF_TO_STRINGS F_STRINGS_TO_BUF F_STR_TO_BUF F_BUF_TO_STR
%token F_SUBBUF F_VERSION F_RANDOM F_TIME F_LOCALTIME F_MTIME
%token F_STRFTIME F_CTIME F_MIN F_MAX F_ABS F_LOOKUP F_METHOD_FLAGS
%token F_METHOD_ACCESS F_SET_METHOD_FLAGS F_SET_METHOD_ACCESS
%token F_METHODOP F_THIS F_DEFINER F_SENDER F_CALLER F_USER F_SET_USER
%token F_TASK_ID F_TICKS_LEFT F_ERROR_FUNC F_TRACEBACK F_ERROR_STR
%token F_ERROR_ARG F_THROW F_RETHROW F_CWRITE F_CWRITEF F_FSTAT
%token F_FREAD F_FCHMOD F_FMKDIR F_FRMDIR F_FILES F_FILE
%token F_FREMOVE F_FRENAME F_FOPEN F_FCLOSE F_FSEEK F_FEOF F_FWRITE
%token F_FFLUSH F_CLOSE_CONNECTION F_CONNECTION F_ADD_VAR F_VARIABLES
%token F_DEL_VAR F_SET_VAR F_GET_VAR F_INHERITED_VAR F_DEFAULT_VAR
%token F_CLEAR_VAR F_ADD_METHOD
%token F_RENAME_METHOD F_METHODS F_FIND_METHOD F_FIND_NEXT_METHOD
%token F_METHOD_BYTECODE F_LIST_METHOD F_DEL_METHOD F_PARENTS
%token F_CHILDREN F_ANCESTORS F_HAS_ANCESTOR F_SIZE
%token F_CREATE F_CHPARENTS F_DESTROY F_DBLOG F_REASSIGN_CONNECTION
%token F_BACKUP F_BINARY_DUMP F_TEXT_DUMP F_EXECUTE F_SHUTDOWN
%token F_BIND_PORT F_UNBIND_PORT F_OPEN_CONNECTION F_SET_HEARTBEAT
%token F_DATA F_SET_OBJNAME F_DEL_OBJNAME F_OBJNAME F_OBJNUM
%token F_NEXT_OBJNUM F_TICK F_HOSTNAME F_IP F_RESUME F_SUSPEND
%token F_TASKS F_TASK_INFO F_CANCEL F_PAUSE F_REFRESH F_STACK F_STATUS
%token F_CACHE_INFO F_BIND_FUNCTION F_UNBIND_FUNCTION F_ATOMIC
%token F_METHOD_INFO F_ENCODE F_DECODE F_SIN F_EXP F_LOG F_COS
%token F_TAN F_SQRT F_ASIN F_ACOS F_ATAN F_POW F_ATAN2 F_CONFIG F_ROUND
%token F_FLUSH OP_HANDLED_FROB F_VALUE F_HANDLER F_CALLINGMETHOD

/* Reserved for future use. */
/*%token FORK*/

/*
// LAST_TOKEN tells opcodes.c how much space to allocate
// for the opcodes table.
*/
%token LAST_TOKEN

/*
// ------------------------------------------------------------
//
// Grammar rules
//
*/

%%

method	: ovdecl argdecl vars stmtlist	{ prog = make_prog($1, $2, $3, $4); }
	;

ovdecl	: /* nothing */			{ $$ = 1; }
	| DISALLOW_OVERRIDES ';'	{ $$ = 0; }
	;

argdecl	: /* nothing */			{ $$ = arguments(NULL, NULL); }
	| ARG '[' IDENT ']' ';'		{ $$ = arguments(NULL, $3); }
	| ARG idlist ';'		{ $$ = arguments($2, NULL); }
	| ARG idlist ',' '[' IDENT ']' ';'
					{ $$ = arguments($2, $5); }
	| ARG '@' IDENT ';'		{ $$ = arguments(NULL, $3); }
	| ARG idlist ',' '@' IDENT  ';'
					{ $$ = arguments($2, $5); }
	;

vars	: /* nothing */			{ $$ = NULL; }
	| VAR idlist ';'		{ $$ = $2; }
	;

idlist	: IDENT				{ $$ = id_list($1, NULL); }
	| idlist ',' IDENT		{ $$ = id_list($3, $1); }
	;

errors	: ANY				{ $$ = NULL; }
	| errlist			{ $$ = $1; }
	;

errlist	: T_ERROR			{ $$ = id_list($1, NULL); }
	| errlist ',' T_ERROR		{ $$ = id_list($3, $1); }
	;

compound: '{' stmtlist '}'		{ $$ = $2; }
	;

stmtlist: /* nothing */			{ $$ = NULL; }
	| stmts				{ $$ = $1; }
	;

stmts	: stmt				{ $$ = stmt_list($1, NULL); }
	| stmts stmt			{ $$ = stmt_list($2, $1); }
	;

stmt	: COMMENT			{ $$ = comment_stmt($1); }
	| ';'				{ $$ = noop_stmt(); }
	| expr ';'			{ $$ = expr_stmt($1); }
	| compound			{ $$ = compound_stmt($1); }
	| if %prec LOWER_THAN_ELSE	{ $$ = $1; }
	| if ELSE stmt			{ $$ = if_else_stmt($1, $3); }
	| for '[' expr UPTO expr ']' stmt
					{ $$ = for_range_stmt($1, $3, $5,$7); }
	| for '(' expr ')' stmt		{ $$ = for_list_stmt($1, $3, $5); }
	| WHILE '(' expr ')' stmt	{ $$ = while_stmt($3, $5); }
	| SWITCH '(' expr ')' caselist	{ $$ = switch_stmt($3, $5); }
	| BREAK ';'			{ $$ = break_stmt(); }
	| CONTINUE ';'			{ $$ = continue_stmt(); }
	| RETURN ';'			{ $$ = return_stmt(); }
	| RETURN expr ';'		{ $$ = return_expr_stmt($2); }
	| CATCH errors stmt %prec LOWER_THAN_WITH
					{ $$ = catch_stmt($2, $3, NULL); }
	| CATCH errors stmt WITH HANDLER stmt
					{ $$ = catch_stmt($2, $3, $6); }
	| CATCH errors stmt WITH stmt
					{ $$ = catch_stmt($2, $3, $5); }
	| error ';'			{ yyerrok; $$ = NULL; }
	;

if	: IF '(' expr ')' stmt		{ $$ = if_stmt($3, $5); }
	;

for	: FOR IDENT OP_IN			{ $$ = $2; }
	;

caselist: '{' '}'			{ $$ = NULL; }
	| '{' cases '}'			{ $$ = $2; }
	;

cases	: case_ent			{ $$ = case_list($1, NULL); }
	| cases case_ent		{ $$ = case_list($2, $1); }
	;

case_ent: CASE cvals ':' stmtlist	{ $$ = case_entry($2, $4); }
	| DEFAULT ':' stmtlist		{ $$ = case_entry(NULL, $3); }
	;

handler : '>'                           { $$ = NULL; }
        | ',' expr '>'                  { $$ = $2; }
        ;

expr	: INTEGER			{ $$ = integer_expr($1); }
	| FLOAT				{ $$ = float_expr($1); }
	| STRING			{ $$ = string_expr($1); }
	| OBJNUM			{ $$ = objnum_expr($1); }
	| OBJNAME			{ $$ = objname_expr($1); }
	| SYMBOL			{ $$ = symbol_expr($1); }
	| T_ERROR				{ $$ = error_expr($1); }
	| IDENT				{ $$ = var_expr($1); }
	| IDENT '(' args ')'		{ $$ = function_call_expr($1, $3); }
	| PASS '(' args ')'		{ $$ = pass_expr($3); }
	| expr '.' IDENT '(' args ')'	{ $$ = message_expr($1, $3, $5); }
	| '.' IDENT '(' args ')'	{ $$ = message_expr(NULL, $2, $4); }
	| expr '.' '(' expr ')' '(' args ')'
					{ $$ = expr_message_expr($1, $4, $7); }
	| '.' '(' expr ')' '(' args ')'
					{ $$ = expr_message_expr(NULL, $3, $6);}
	| '[' args ']'			{ $$ = list_expr($2); }
	| START_DICT args ']'		{ $$ = dict_expr($2); }
	| START_BUFFER args ']'		{ $$ = buffer_expr($2); }
	| '<' expr ',' expr handler     { $$ = frob_expr($2, $4, $5); }
	| expr '[' expr ']'		{ $$ = index_expr($1, $3); }
	| INCREMENT IDENT               { $$ = indecr_expr(P_INCREMENT, $2); }
	| DECREMENT IDENT        	{ $$ = indecr_expr(P_DECREMENT, $2); }
	| IDENT INCREMENT               { $$ = indecr_expr(INCREMENT, $1); }
	| IDENT DECREMENT       	{ $$ = indecr_expr(DECREMENT, $1); }
	| '!' expr			{ $$ = unary_expr('!', $2); }
	| '-' expr %prec '!'		{ $$ = unary_expr(NEG, $2); }
	| '+' expr %prec '!'		{ $$ = $2; }
        | expr '*' expr                 { $$ = binary_expr('*', $1, $3); }
	| expr '/' expr			{ $$ = binary_expr('/', $1, $3); }
	| expr '%' expr			{ $$ = binary_expr('%', $1, $3); }
	| expr '+' expr			{ $$ = binary_expr('+', $1, $3); }
	| expr '-' expr			{ $$ = binary_expr('-', $1, $3); }
	| expr EQ expr			{ $$ = binary_expr(EQ, $1, $3); }
	| expr NE expr			{ $$ = binary_expr(NE, $1, $3); }
	| expr '>' expr			{ $$ = binary_expr('>', $1, $3); }
	| expr GE expr			{ $$ = binary_expr(GE, $1, $3); }
	| expr '<' expr			{ $$ = binary_expr('<', $1, $3); }
	| expr LE expr			{ $$ = binary_expr(LE, $1, $3); }
	| expr OP_IN expr			{ $$ = binary_expr(OP_IN, $1, $3); }
	| expr AND expr			{ $$ = and_expr($1, $3); }
	| expr OR expr			{ $$ = or_expr($1, $3); }
	| expr OP_COND_IF expr ':' expr	{ $$ = cond_expr($1, $3, $5); }
	| map '(' expr ')' TO '(' expr ')'  { $$ = map_expr($3,$1,$7,OP_MAP); }
        | map '[' expr UPTO expr ']' TO '(' expr ')'  { $$ = map_range_expr($3,$5,$1,$9,OP_MAP_RANGE); }
	| hash '(' expr ')' TO '(' expr ')'  { $$ = map_expr($3,$1,$7,OP_MAPHASH); }
        | hash '[' expr UPTO expr ']' TO '(' expr ')'  { $$ = map_range_expr($3,$5,$1,$9,OP_MAPHASH_RANGE); }
	| find '(' expr ')' WHERE '(' expr ')' { $$ = map_expr($3,$1,$7,OP_FIND); }
	| find '[' expr UPTO expr ']' WHERE '(' expr ')'  { $$ = map_range_expr($3,$5,$1,$9,OP_FIND_RANGE); }
	| filter '(' expr ')' WHERE '(' expr ')' { $$ = map_expr($3,$1,$7,OP_FILTER); }
	| filter '[' expr UPTO expr ']' WHERE '(' expr ')'  { $$ = map_range_expr($3,$5,$1,$9,OP_FILTER_RANGE); }
	| expr OP_COND_IF expr OP_COND_OTHER_ELSE expr	{ $$ = cond_expr($1, $3, $5); }
	| IDENT MULT_EQ expr		{ $$ = doeq_expr(MULT_EQ, $1, $3); }
	| IDENT DIV_EQ expr		{ $$ = doeq_expr(DIV_EQ, $1, $3); }
	| IDENT PLUS_EQ expr		{ $$ = doeq_expr(PLUS_EQ, $1, $3); }
	| IDENT MINUS_EQ expr		{ $$ = doeq_expr(MINUS_EQ, $1, $3); }
	| IDENT OPTIONAL_ASSIGN expr	{ $$ = opt_expr($1, $3); }
	| expr OP_ASSIGN expr		{ $$ = assign_expr($1, $3); }
	| '(' expr ')'			{ $$ = $2; }
        | CRITLEFT expr CRITRIGHT       { $$ = critical_expr($2); }
	| PROPLEFT expr PROPRIGHT	{ $$ = propagate_expr($2); }
	;

map     : OP_MAP IDENT OP_IN               { $$ = $2; }
	;

find    : OP_FIND IDENT OP_IN              { $$ = $2; }
	;

filter  : OP_FILTER IDENT OP_IN            { $$ = $2; }
	;

hash	: OP_MAPHASH IDENT OP_IN		{ $$ = $2; }
	;

sexpr	: expr				{ $$ = $1; }
	| '@' expr			{ $$ = splice_expr($2); }
	;

args	: /* nothing */			{ $$ = NULL; }
	| arglist			{ $$ = $1; }
	;

arglist	: sexpr				{ $$ = expr_list($1, NULL); }
	| arglist ',' sexpr		{ $$ = expr_list($3, $1); }
	;

rexpr	: expr				{ $$ = $1; }
	| expr UPTO expr		{ $$ = range_expr($1, $3); }
	;

cvals	: rexpr				{ $$ = expr_list($1, NULL); }
	| cvals ',' rexpr		{ $$ = expr_list($3, $1); }
	;

%%

/*
// ------------------------------------------------------------
//
// Additional code
//
*/

Method * compile(Obj * object, cList * code, cList ** error_ret) {
    Method * method = NULL;

    /* Initialize compiler globals. */
    errors = list_new(0);
    lex_start(code);

    /* Parse text.  This sets prog if successful. */
    yyparse();

    if (!errors->len) {
	/* No errors in parsing.  Compile to linear code.  method will be
	 * NULL if unsuccessful. */
	method = generate_method(prog, object);
    }

    /* Free up all temporary storage we allocated during compilation. */
    pfree(compiler_pile);

    /* error_ret gets reference count on errors. */
    *error_ret = errors;
    return method;
}

void compiler_error(Int lineno, char *fmt, ...)
{
    va_list arg;
    cStr * errstr, * line;
    cData d;

    va_start(arg, fmt);
    errstr = vformat(fmt, arg);

    if (lineno == -1) {
	line = errstr;
    } else {
	line = format("Line %d: %s", lineno, errstr->s);
	string_discard(errstr);
    }

    d.type = STRING;
    d.u.str = line;
    errors = list_add(errors, &d);

    string_discard(line);
    va_end(arg);
}

Int no_errors(void) {
    return (errors->len == 0);
}

static void yyerror(char * s) {
    compiler_error(cur_lineno(), s);
}

#undef _grammar_y_
