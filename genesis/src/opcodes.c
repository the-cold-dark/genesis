/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include <ctype.h>
#include "cdc_pcode.h"
#include "operators.h"
#include "functions.h"
#include "util.h"

#define FDEF(_def_, _str_, _name_)  { _def_, _str_, CAT(func_, _name_) },       

#define NUM_OPERATORS (sizeof(op_info) / sizeof(*op_info))

Op_info op_table[LAST_TOKEN];

static Int first_function;

static Op_info op_info[] = {
    { COMMENT,          "COMMENT",         op_comment, STRING },
    { POP,              "POP",             op_pop },
    { SET_LOCAL,        "SET_LOCAL",       op_set_local, VAR },
    { SET_OBJ_VAR,      "SET_OBJ_VAR",     op_set_obj_var, IDENT },
    { IF,               "IF",              op_if, JUMP },
    { IF_ELSE,          "IF_ELSE",         op_if, JUMP },
    { ELSE,             "ELSE",            op_else, JUMP },
    { FOR_RANGE,        "FOR_RANGE",       op_for_range, JUMP, INTEGER },
    { FOR_LIST,         "FOR_LIST",        op_for_list, JUMP, INTEGER },
    { WHILE,            "WHILE",           op_while, JUMP, JUMP },
    { SWITCH,           "SWITCH",          op_switch, JUMP },
    { CASE_VALUE,       "CASE_VALUE",      op_case_value, JUMP },
    { CASE_RANGE,       "CASE_RANGE",      op_case_range, JUMP },
    { LAST_CASE_VALUE,  "LAST_CASE_VALUE", op_last_case_value, JUMP },
    { LAST_CASE_RANGE,  "LAST_CASE_RANGE", op_last_case_range, JUMP },
    { END_CASE,         "END_CASE",        op_end_case, JUMP },
    { DEFAULT,          "DEFAULT",         op_default },
    { END,              "END",             op_end, JUMP },
    { BREAK,            "BREAK",           op_break, JUMP, INTEGER },
    { CONTINUE,         "CONTINUE",        op_continue, JUMP, INTEGER },
    { RETURN,           "RETURN",          op_return },
    { RETURN_EXPR,      "RETURN_EXPR",     op_return_expr },
    { CATCH,            "CATCH",           op_catch, JUMP, T_ERROR },
    { CATCH_END,        "CATCH_END",       op_catch_end, JUMP },
    { HANDLER_END,      "HANDLER_END",     op_handler_end },
    { ZERO,             "ZERO",            op_zero },
    { ONE,              "ONE",             op_one },
    { INTEGER,          "INTEGER",         op_integer, INTEGER },

    /* By the time it examines the arg, the FLOAT has already been
       cast into an INTEGER, so we just need to let it know its an INT */
    { FLOAT,            "FLOAT",           op_float, INTEGER },
    { STRING,           "STRING",          op_string, STRING },
    { OBJNUM,           "OBJNUM",          op_objnum, INTEGER },
    { SYMBOL,           "SYMBOL",          op_symbol, IDENT },
    { T_ERROR,            "ERROR",         op_error, IDENT },
    { OBJNAME,          "OBJNAME",         op_objname, IDENT },
    { GET_LOCAL,        "GET_LOCAL",       op_get_local, VAR },
    { GET_OBJ_VAR,      "GET_OBJ_VAR",     op_get_obj_var, IDENT },
    { START_ARGS,       "START_ARGS",      op_start_args },
    { PASS,             "PASS",            op_pass },
    { CALL_METHOD,      "CALL_METHOD",     op_message, IDENT },
    { EXPR_CALL_METHOD, "EXPR_CALL_METHOD",    op_expr_message },
    { LIST,             "LIST",            op_list },
    { DICT,             "DICT",            op_dict },
    { BUFFER,           "BUFFER",          op_buffer },
    { FROB,             "FROB",            op_frob },
    { OP_HANDLED_FROB,  "FROB",            op_handled_frob },
    { INDEX,            "INDEX",           op_index },
    { AND,              "AND",             op_and, JUMP },
    { OR,               "OR",              op_or, JUMP },
    { CONDITIONAL,      "CONDITIONAL",     op_if, JUMP },
    { OP_MAP,           "OP_MAP",          op_map, JUMP, VAR },
    { OP_MAP_RANGE,     "OP_MAP_RANGE",    op_map_range, JUMP, VAR },
    { OP_FIND,          "OP_FIND",         op_map, JUMP, VAR },
    { OP_FIND_RANGE,    "OP_FIND_RANGE",   op_map_range, JUMP, VAR },
    { OP_FILTER,        "OP_FILTER",       op_map, JUMP, VAR },
    { OP_FILTER_RANGE,  "OP_FILTER_RANGE", op_map_range, JUMP, VAR },
    { OP_MAPHASH,       "OP_MAPHASH",      op_map, JUMP, VAR },
    { OP_MAPHASH_RANGE, "OP_MAPHASH_RANGE",op_map_range, JUMP, VAR },
    { SPLICE,           "SPLICE",          op_splice },
    { CRITICAL,         "CRITICAL",        op_critical, JUMP },
    { CRITICAL_END,     "CRITICAL_END",    op_critical_end },
    { PROPAGATE,        "PROPAGATE",       op_propagate, JUMP },
    { PROPAGATE_END,    "PROPAGATE_END",   op_propagate_end },

    /* Arithmetic and relational operators (arithop.c). */
    { '!',           "!",               op_not },
    { NEG,              "NEG",             op_negate },
    { '*',              "*",               op_multiply },
    { '/',              "/",               op_divide },
    { '%',              "%",               op_modulo },
    { '+',              "+",               op_add },
    { SPLICE_ADD,       "SPLICE_ADD",      op_splice_add },
    { '-',              "-",               op_subtract },
    { EQ,               "EQ",              op_equal },
    { NE,               "NE",              op_not_equal },
    { '>',              ">",               op_greater },
    { GE,               ">=",              op_greater_or_equal },
    { '<',              "<",               op_less },
    { LE,               "<=",              op_less_or_equal },
    { OP_IN,            "IN",              op_in },
    { P_INCREMENT,      "P_INCREMENT",     op_p_increment },
    { P_DECREMENT,      "P_DECREMENT",     op_p_decrement },
    { INCREMENT,        "INCREMENT",       op_increment },
    { DECREMENT,        "DECREMENT",       op_decrement },
    { MULT_EQ,          "MULT_EQ",         op_doeq_multiply },
    { DIV_EQ,           "DIV_EQ",          op_doeq_divide },
    { PLUS_EQ,          "PLUS_EQ",         op_doeq_add },
    { MINUS_EQ,         "MINUS_EQ",        op_doeq_subtract },
    { OPTIONAL_ASSIGN,  "OPTIONAL_ASSIGN", op_optional_assign, JUMP },
    { OPTIONAL_END,     "OPTIONAL_END",    op_optional_end },
    { SCATTER_START,    "SCATTER_START",   op_scatter_start },
    { SCATTER_END,      "SCATTER_END",     0},

    /* Object variable functions */
    FDEF(F_ADD_VAR,       "add_var",   add_var)
    FDEF(F_DEL_VAR,       "del_var",   del_var)
    FDEF(F_SET_VAR,       "set_var",   set_var)
    FDEF(F_GET_VAR,       "get_var",   get_var)
    FDEF(F_INHERITED_VAR, "inherited_var", inherited_var)
    FDEF(F_DEFAULT_VAR,   "default_var", default_var)
    FDEF(F_CLEAR_VAR,     "clear_var", clear_var)
    FDEF(F_VARIABLES,     "variables", variables)

    /* debugger */
    FDEF(F_DEBUG_CALLERS, "debug_callers", debug_callers)
    FDEF(F_CALL_TRACE,    "call_trace",    call_trace)

    /* Object method functions */
    FDEF(F_LIST_METHOD,       "list_method",       list_method)
    FDEF(F_ADD_METHOD,        "add_method",        add_method)
    FDEF(F_DEL_METHOD,        "del_method",        del_method)
    FDEF(F_METHOD_BYTECODE,   "method_bytecode",   method_bytecode)
    FDEF(F_RENAME_METHOD,     "rename_method",     rename_method)
    FDEF(F_SET_METHOD_FLAGS,  "set_method_flags",  set_method_flags)
    FDEF(F_SET_METHOD_ACCESS, "set_method_access", set_method_access)
    FDEF(F_METHOD_INFO,       "method_info",       method_info)
    FDEF(F_METHOD_FLAGS,      "method_flags",      method_flags)
    FDEF(F_METHOD_ACCESS,     "method_access",     method_access)
    FDEF(F_METHODS,           "methods",           methods)
    FDEF(F_FIND_METHOD,       "find_method",       find_method)
    FDEF(F_FIND_NEXT_METHOD,  "find_next_method",  find_next_method)

    /* Object functions */
    FDEF(F_PARENTS,      "parents",      parents)
    FDEF(F_CHILDREN,     "children",     children)
    FDEF(F_ANCESTORS,    "ancestors",    ancestors)
    FDEF(F_HAS_ANCESTOR, "has_ancestor", has_ancestor)
    FDEF(F_SIZE,         "size",         size)
    FDEF(F_CREATE,       "create",       create)
    FDEF(F_CHPARENTS,    "chparents",    chparents)
    FDEF(F_DESTROY,      "destroy",      destroy)
    FDEF(F_SET_OBJNAME,  "set_objname",  set_objname)
    FDEF(F_DEL_OBJNAME,  "del_objname",  del_objname)
    FDEF(F_OBJNAME,      "objname",      objname)
    FDEF(F_OBJNUM,       "objnum",       objnum)
    FDEF(F_LOOKUP,       "lookup",       lookup)
    FDEF(F_DATA,         "data",         data)

    /* System functions */
    { F_DBLOG,             "dblog",               func_dblog },
    { F_BACKUP,            "backup",              func_backup },
    { F_SHUTDOWN,          "shutdown",            func_shutdown },
    { F_SET_HEARTBEAT,     "set_heartbeat",       func_set_heartbeat },
    { F_CACHE_INFO,       "cache_info",           func_cache_info },

    /* Task/Frame functions */
    { F_TICK,             "tick",                 func_tick },
    { F_RESUME,           "resume",               func_resume },
    { F_SUSPEND,          "suspend",              func_suspend },
    { F_TASKS,            "tasks",                func_tasks },
    { F_TASK_INFO,        "task_info",            func_task_info },
    { F_TASK_ID,          "task_id",              func_task_id },
    { F_CANCEL,           "cancel",               func_cancel },
    { F_PAUSE,            "pause",                func_pause },
    { F_REFRESH,          "refresh",              func_refresh },
    { F_TICKS_LEFT,       "ticks_left",           func_ticks_left },
    { F_CALLINGMETHOD,    "calling_method",       func_calling_method },
    { F_METHODOP,         "method",               func_method },
    { F_THIS,             "this",                 func_this },
    { F_DEFINER,          "definer",              func_definer },
    { F_SENDER,           "sender",               func_sender },
    { F_CALLER,           "caller",               func_caller },
    { F_STACK,            "stack",                func_stack },
    { F_ATOMIC,           "atomic",               func_atomic },
    { F_USER,             "user",                 func_user },
    { F_SET_USER,         "set_user",             func_set_user },

    /* Data/Conversion functions */
    { F_VALID,            "valid",                func_valid },
    { F_TYPE,             "type",                 func_type },
    { F_CLASS,            "class", /* devalued */ func_frob_class },
    { F_FROB_CLASS,       "frob_class",           func_frob_class },
    { F_VALUE,            "frob_value",           func_frob_value },
    { F_HANDLER,          "frob_handler",         func_frob_handler },
    { F_TOINT,            "toint",                func_toint },
    { F_TOFLOAT,          "tofloat",              func_tofloat },
    { F_TOSTR,            "tostr",                func_tostr },
    { F_TOLITERAL,        "toliteral",            func_toliteral },
    { F_FROMLITERAL,      "fromliteral",          func_fromliteral },
    { F_TOOBJNUM,         "toobjnum",             func_toobjnum },
    { F_TOSYM,            "tosym",                func_tosym },
    { F_TOERR,            "toerr",                func_toerr },

    /* Exception functions */
    { F_ERROR_FUNC,       "error",                func_error },
    { F_TRACEBACK,        "traceback",            func_traceback },
    { F_THROW,            "throw",                func_throw },
    { F_RETHROW,          "rethrow",              func_rethrow },

    /* Network control functions */
    { F_REASSIGN_CONNECTION,"reassign_connection",func_reassign_connection },
    { F_BIND_PORT,        "bind_port",            func_bind_port },
    { F_UNBIND_PORT,      "unbind_port",          func_unbind_port },
    { F_OPEN_CONNECTION,  "open_connection",      func_open_connection },
    { F_CLOSE_CONNECTION, "close_connection",     func_close_connection },
    { F_CWRITE,           "cwrite",               func_cwrite },
    { F_CWRITEF,          "cwritef",              func_cwritef },
    { F_CONNECTION,       "connection",           func_connection },

    /* File control functions */
    { F_EXECUTE,          "execute",              func_execute },
    { F_FSTAT,          "fstat",                func_fstat },
    { F_FREAD,          "fread",                func_fread },
    { F_FCHMOD,         "fchmod",               func_fchmod },
    { F_FMKDIR,         "fmkdir",               func_fmkdir },
    { F_FRMDIR,         "frmdir",               func_frmdir },
    { F_FILES,          "files",                func_files },
    { F_FREMOVE,        "fremove",              func_fremove },
    { F_FRENAME,        "frename",              func_frename },
    { F_FOPEN,          "fopen",                func_fopen },
    { F_FCLOSE,         "fclose",               func_fclose },
    { F_FSEEK,          "fseek",                func_fseek },
    { F_FEOF,           "feof",                 func_feof },
    { F_FWRITE,         "fwrite",               func_fwrite },
    { F_FILE,           "file",                 func_file },
    { F_FFLUSH,         "fflush",               func_fflush },

    /* Miscellaneous functions */
    { F_FLUSH,  "anticipate_assignment",     func_anticipate_assignment},
    { F_LOCALTIME,        "localtime",       func_localtime },
    { F_TIME,             "time",            func_time },
    { F_MTIME,            "mtime",           func_mtime },
    { F_CTIME,            "ctime",           func_ctime },
    { F_BIND_FUNCTION,    "bind_function",   func_bind_function },
    { F_UNBIND_FUNCTION,  "unbind_function", func_unbind_function },
    { F_CONFIG,           "config",          func_config },
    { F_RANDOM,           "random",          func_random },
    { F_MIN,            "min",               func_min },
    { F_MAX,            "max",               func_max },
    { F_ABS,            "abs",               func_abs },
    { F_SIN,            "sin",               func_sin },
    { F_EXP,            "exp",               func_exp },
    { F_LOG,            "log",               func_log },
    { F_COS,            "cos",               func_cos },
    { F_TAN,            "tan",               func_tan },
    { F_SQRT,           "sqrt",              func_sqrt },
    { F_ASIN,           "asin",              func_asin },
    { F_ACOS,           "acos",              func_acos },
    { F_ATAN,           "atan",              func_atan },
    { F_POW,            "pow",               func_pow },
    { F_ATAN2,          "atan2",             func_atan2 },
    { F_ROUND,          "round",             func_round },

    /* Operations on strings (stringop.c). */
    { F_STRFMT,           "strfmt",          func_strfmt },
    { F_STRLEN,           "strlen",          func_strlen },
    { F_SUBSTR,           "substr",          func_substr },
    { F_EXPLODE,          "explode",         func_explode },
    { F_STRSUB,           "strsub",          func_strsub },
    { F_PAD,              "pad",             func_pad },
    { F_MATCH_BEGIN,      "match_begin",     func_match_begin },
    { F_MATCH_TEMPLATE,   "match_template",  func_match_template },
    { F_MATCH_PATTERN,    "match_pattern",   func_match_pattern },
    { F_MATCH_REGEXP,     "match_regexp",    func_match_regexp },
    { F_REGEXP,           "regexp",          func_regexp },
    { F_SPLIT,            "split",           func_split },
    { F_CRYPT,            "crypt",           func_crypt },
    { F_MATCH_CRYPTED,    "match_crypted",   func_match_crypted },
    { F_UPPERCASE,        "uppercase",       func_uppercase },
    { F_LOWERCASE,        "lowercase",       func_lowercase },
    { F_STRCMP,           "strcmp",          func_strcmp },
    { F_STRSED,           "strsed",          func_strsed },
    { F_STRGRAFT,         "strgraft",        func_strgraft },
    { F_STRIDX,           "stridx",          func_stridx },

    /* List manipulation (listop.c). */
    { F_LISTLEN,          "listlen",         func_listlen },
    { F_SUBLIST,          "sublist",         func_sublist },
    { F_INSERT,           "insert",          func_insert },
    { F_REPLACE,          "replace",         func_replace },
    { F_DELETE,           "delete",          func_delete },
    { F_SETADD,           "setadd",          func_setadd },
    { F_SETREMOVE,        "setremove",       func_setremove },
    { F_UNION,            "union",           func_union },
    { F_LISTGRAFT,        "listgraft",       func_listgraft },
    { F_JOIN,             "join",            func_join },
    { F_LISTIDX,          "listidx",         func_listidx },

    /* Dictionary manipulation (dictop.c). */
    { F_DICT_VALUES,      "dict_values",     func_dict_values },
    { F_DICT_KEYS,        "dict_keys",       func_dict_keys },
    { F_DICT_ADD,         "dict_add",        func_dict_add },
    { F_DICT_DEL,         "dict_del",        func_dict_del },
    { F_DICT_CONTAINS,    "dict_contains",   func_dict_contains },
    { F_DICT_UNION,       "dict_union",      func_dict_union },

    /* Buffer manipulation (bufferop.c). */
    { F_BUFLEN,           "buflen",          func_buflen },
    { F_BUFIDX,           "bufidx",          func_bufidx },
    { F_BUF_REPLACE,      "buf_replace",     func_buf_replace },
    { F_BUF_TO_STRINGS,   "buf_to_strings",  func_buf_to_strings },
    { F_BUF_TO_STR,       "buf_to_str",      func_buf_to_str },
    { F_STRINGS_TO_BUF,   "strings_to_buf",  func_strings_to_buf },
    { F_STR_TO_BUF,       "str_to_buf",      func_str_to_buf },
    { F_SUBBUF,           "subbuf",          func_subbuf },
    { F_BUFGRAFT,         "bufgraft",        func_bufgraft },
};

void init_op_table(void) {
    Int i;

    for (i = 0; i < NUM_OPERATORS; i++) {
        op_info[i].binding = INV_OBJNUM;
        op_info[i].symbol = ident_get(op_info[i].name);
        op_table[op_info[i].opcode] = op_info[i];
    }

    /* Look for first opcode with a lowercase name to find the first
     * function. */
    for (i = 0; i < NUM_OPERATORS; i++) {
        if (islower(*op_info[i].name))
            break;
    }
    first_function = i;
}

Int find_function(char *name) {
    Int i;

    for (i = first_function; i < NUM_OPERATORS; i++) {
        if (strcmp(op_info[i].name, name) == 0)
            return op_info[i].opcode;
    }

    return -1;
}

