/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include <ctype.h>
#include "cdc_pcode.h"
#include "operators.h"
#include "functions.h"
#include "util.h"

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

    { FLOAT_ZERO,       "FLOAT_ZERO",      op_float_zero },
    { FLOAT_ONE,        "FLOAT_ONE",       op_float_one },

#if defined(USE_BIG_FLOATS) && !defined(USE_BIG_NUMBERS)
    /* Big floats are the size of two ints */
    { FLOAT,            "FLOAT",           op_float, INTEGER, INTEGER },
#else
    /* By the time it examines the arg, the FLOAT has already been
       cast into an INTEGER, so we just need to let it know its an INT */
    { FLOAT,            "FLOAT",           op_float, INTEGER },
#endif
    { STRING,           "STRING",          op_string, STRING },
    { OBJNUM,           "OBJNUM",          op_objnum, INTEGER },
    { SYMBOL,           "SYMBOL",          op_symbol, IDENT },
    { T_ERROR,          "ERROR",           op_error, IDENT },
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
    { '!',              "!",               op_not },
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

    /* Object variable functions, MUST be in alpha order */
    { F_ABS,                   "abs",                   func_abs },
    { F_ACOS,                  "acos",                  func_acos },
    { F_ADD_METHOD,            "add_method",            func_add_method },
    { F_ADD_VAR,               "add_var",               func_add_var },
    { F_ANCESTORS,             "ancestors",             func_ancestors },
    { F_ANTICIPATE_ASSIGNMENT, "anticipate_assignment", func_anticipate_assignment },
    { F_ASIN,                  "asin",                  func_asin },
    { F_ATAN,                  "atan",                  func_atan },
    { F_ATAN2,                 "atan2",                 func_atan2 },
    { F_ATOMIC,                "atomic",                func_atomic },
    { F_BACKUP,                "backup",                func_backup },
    { F_BIND_FUNCTION,         "bind_function",         func_bind_function },
    { F_BIND_PORT,             "bind_port",             func_bind_port },
    { F_BUF_REPLACE,           "buf_replace",           func_buf_replace },
    { F_BUF_TO_STR,            "buf_to_str",            func_buf_to_str },
    { F_BUF_TO_STRINGS,        "buf_to_strings",        func_buf_to_strings },
    { F_BUFGRAFT,              "bufgraft",              func_bufgraft },
    { F_BUFIDX,                "bufidx",                func_bufidx },
    { F_BUFLEN,                "buflen",                func_buflen },
    { F_BUFSUB,                "bufsub",                func_bufsub },
    { F_CACHE_INFO,            "cache_info",            func_cache_info },
    { F_CACHE_STATS,           "cache_stats",           func_cache_stats },
    { F_CALL_TRACE,            "call_trace",            func_call_trace },
    { F_CALLER,                "caller",                func_caller },
    { F_CALLING_METHOD,        "calling_method",        func_calling_method },
    { F_CANCEL,                "cancel",                func_cancel },
    { F_CHILDREN,              "children",              func_children },
    { F_CHPARENTS,             "chparents",             func_chparents },
    { F_CLASS,                 "class",                 func_frob_class },   /* devalued, see frob_class */
    { F_CLEAR_VAR,             "clear_var",             func_clear_var },
    { F_CLOSE_CONNECTION,      "close_connection",      func_close_connection },
    { F_CONFIG,                "config",                func_config },
    { F_CONNECTION,            "connection",            func_connection },
    { F_COS,                   "cos",                   func_cos },
    { F_CREATE,                "create",                func_create },
    { F_CRYPT,                 "crypt",                 func_crypt },
    { F_CTIME,                 "ctime",                 func_ctime },
    { F_CWRITE,                "cwrite",                func_cwrite },
    { F_CWRITEF,               "cwritef",               func_cwritef },
    { F_DATA,                  "data",                  func_data },
    { F_DBLOG,                 "dblog",                 func_dblog },
    { F_DEBUG_CALLERS,         "debug_callers",         func_debug_callers },
    { F_DEFAULT_VAR,           "default_var",           func_default_var },
    { F_DEFINER,               "definer",               func_definer },
    { F_DEL_METHOD,            "del_method",            func_del_method },
    { F_DEL_OBJNAME,           "del_objname",           func_del_objname },
    { F_DEL_VAR,               "del_var",               func_del_var },
    { F_DELETE,                "delete",                func_delete },
    { F_DESTROY,               "destroy",               func_destroy },
    { F_DICT_ADD,              "dict_add",              func_dict_add },
    { F_DICT_CONTAINS,         "dict_contains",         func_dict_contains },
    { F_DICT_DEL,              "dict_del",              func_dict_del },
    { F_DICT_KEYS,             "dict_keys",             func_dict_keys },
    { F_DICT_UNION,            "dict_union",            func_dict_union },
    { F_DICT_VALUES,           "dict_values",           func_dict_values },
    { F_ERROR_FUNC,            "error",                 func_error },
    { F_ERROR_DATA,            "error_data",            func_error_data },
    { F_ERROR_MESSAGE,         "error_message",         func_error_message },
    { F_EXECUTE,               "execute",               func_execute },
    { F_EXP,                   "exp",                   func_exp },
    { F_EXPLODE,               "explode",               func_explode },
    { F_EXPLODE_QUOTED,        "explode_quoted",        func_explode_quoted },
    { F_FCHMOD,                "fchmod",                func_fchmod },
    { F_FCLOSE,                "fclose",                func_fclose },
    { F_FEOF,                  "feof",                  func_feof },
    { F_FFLUSH,                "fflush",                func_fflush },
    { F_FILE,                  "file",                  func_file },
    { F_FILES,                 "files",                 func_files },
    { F_FIND_METHOD,           "find_method",           func_find_method },
    { F_FIND_NEXT_METHOD,      "find_next_method",      func_find_next_method },
    { F_FMKDIR,                "fmkdir",                func_fmkdir },
    { F_FOPEN,                 "fopen",                 func_fopen },
    { F_FREAD,                 "fread",                 func_fread },
    { F_FREMOVE,               "fremove",               func_fremove },
    { F_FRENAME,               "frename",               func_frename },
    { F_FRMDIR,                "frmdir",                func_frmdir },
    { F_FROB_CLASS,            "frob_class",            func_frob_class },
    { F_FROB_HANDLER,          "frob_handler",          func_frob_handler },
    { F_FROB_VALUE,            "frob_value",            func_frob_value },
    { F_FROMLITERAL,           "fromliteral",           func_fromliteral },
    { F_FSEEK,                 "fseek",                 func_fseek },
    { F_FSTAT,                 "fstat",                 func_fstat },
    { F_FWRITE,                "fwrite",                func_fwrite },
    { F_GET_VAR,               "get_var",               func_get_var },
    { F_HAS_ANCESTOR,          "has_ancestor",          func_has_ancestor },
    { F_HAS_METHOD,            "has_method",            func_has_method },
    { F_INHERITED_VAR,         "inherited_var",         func_inherited_var },
    { F_INSERT,                "insert",                func_insert },
    { F_JOIN,                  "join",                  func_join },
    { F_LIST_METHOD,           "list_method",           func_list_method },
    { F_LISTGRAFT,             "listgraft",             func_listgraft },
    { F_LISTIDX,               "listidx",               func_listidx },
    { F_LISTLEN,               "listlen",               func_listlen },
    { F_LOCALTIME,             "localtime",             func_localtime },
    { F_LOG,                   "log",                   func_log },
    { F_LOOKUP,                "lookup",                func_lookup },
    { F_LOWERCASE,             "lowercase",             func_lowercase },
    { F_MATCH_BEGIN,           "match_begin",           func_match_begin },
    { F_MATCH_CRYPTED,         "match_crypted",         func_match_crypted },
    { F_MATCH_PATTERN,         "match_pattern",         func_match_pattern },
    { F_MATCH_REGEXP,          "match_regexp",          func_match_regexp },
    { F_MATCH_TEMPLATE,        "match_template",        func_match_template },
    { F_MAX,                   "max",                   func_max },
    { F_MEMORY_SIZE,           "memory_size",           func_memory_size },
    { F_METHODOP,              "method",                func_method },
    { F_METHOD_ACCESS,         "method_access",         func_method_access },
    { F_METHOD_BYTECODE,       "method_bytecode",       func_method_bytecode },
    { F_METHOD_FLAGS,          "method_flags",          func_method_flags },
    { F_METHOD_INFO,           "method_info",           func_method_info },
    { F_METHODS,               "methods",               func_methods },
    { F_MIN,                   "min",                   func_min },
    { F_MTIME,                 "mtime",                 func_mtime },
    { F_OBJNAME,               "objname",               func_objname },
    { F_OBJNUM,                "objnum",                func_objnum },
    { F_OPEN_CONNECTION,       "open_connection",       func_open_connection },
    { F_PAD,                   "pad",                   func_pad },
    { F_PARENTS,               "parents",               func_parents },
    { F_PAUSE,                 "pause",                 func_pause },
    { F_POW,                   "pow",                   func_pow },
    { F_RANDOM,                "random",                func_random },
    { F_REASSIGN_CONNECTION,   "reassign_connection",   func_reassign_connection },
    { F_REFRESH,               "refresh",               func_refresh },
    { F_REGEXP,                "regexp",                func_regexp },
    { F_RENAME_METHOD,         "rename_method",         func_rename_method },
    { F_REPLACE,               "replace",               func_replace },
    { F_RESUME,                "resume",                func_resume },
    { F_RETHROW,               "rethrow",               func_rethrow },
    { F_ROUND,                 "round",                 func_round },
    { F_SENDER,                "sender",                func_sender },
    { F_SET_HEARTBEAT,         "set_heartbeat",         func_set_heartbeat },
    { F_SET_METHOD_ACCESS,     "set_method_access",     func_set_method_access },
    { F_SET_METHOD_FLAGS,      "set_method_flags",      func_set_method_flags },
    { F_SET_OBJNAME,           "set_objname",           func_set_objname },
    { F_SET_USER,              "set_user",              func_set_user },
    { F_SET_VAR,               "set_var",               func_set_var },
    { F_SETADD,                "setadd",                func_setadd },
    { F_SETREMOVE,             "setremove",             func_setremove },
    { F_SHUTDOWN,              "shutdown",              func_shutdown },
    { F_SIN,                   "sin",                   func_sin },
    { F_SIZE,                  "size",                  func_size },
    { F_SPLIT,                 "split",                 func_split },
    { F_SQRT,                  "sqrt",                  func_sqrt },
    { F_STACK,                 "stack",                 func_stack },
    { F_STR_TO_BUF,            "str_to_buf",            func_str_to_buf },
    { F_STRCMP,                "strcmp",                func_strcmp },
    { F_STRFMT,                "strfmt",                func_strfmt },
    { F_STRGRAFT,              "strgraft",              func_strgraft },
    { F_STRIDX,                "stridx",                func_stridx },
    { F_STRINGS_TO_BUF,        "strings_to_buf",        func_strings_to_buf },
    { F_STRLEN,                "strlen",                func_strlen },
    { F_STRSED,                "strsed",                func_strsed },
    { F_STRSUB,                "strsub",                func_strsub },
    { F_SUBBUF,                "subbuf",                func_subbuf },
    { F_SUBLIST,               "sublist",               func_sublist },
    { F_SUBSTR,                "substr",                func_substr },
    { F_SUSPEND,               "suspend",               func_suspend },
    { F_SYNC,                  "sync",                  func_sync },
    { F_TAN,                   "tan",                   func_tan },
    { F_TASK_ID,               "task_id",               func_task_id },
    { F_TASK_INFO,             "task_info",             func_task_info },
    { F_TASKS,                 "tasks",                 func_tasks },
    { F_THIS,                  "this",                  func_this },
    { F_THROW,                 "throw",                 func_throw },
    { F_TICK,                  "tick",                  func_tick },
    { F_TICKS_LEFT,            "ticks_left",            func_ticks_left },
    { F_TIME,                  "time",                  func_time },
    { F_TOERR,                 "toerr",                 func_toerr },
    { F_TOFLOAT,               "tofloat",               func_tofloat },
    { F_TOINT,                 "toint",                 func_toint },
    { F_TOLITERAL,             "toliteral",             func_toliteral },
    { F_TOOBJNUM,              "toobjnum",              func_toobjnum },
    { F_TOSTR,                 "tostr",                 func_tostr },
    { F_TOSYM,                 "tosym",                 func_tosym },
    { F_TRACEBACK,             "traceback",             func_traceback },
    { F_TYPE,                  "type",                  func_type },
    { F_UNBIND_FUNCTION,       "unbind_function",       func_unbind_function },
    { F_UNBIND_PORT,           "unbind_port",           func_unbind_port },
    { F_UNION,                 "union",                 func_union },
    { F_UPPERCASE,             "uppercase",             func_uppercase },
    { F_USER,                  "user",                  func_user },
    { F_VALID,                 "valid",                 func_valid },
    { F_VARIABLES,             "variables",             func_variables }
};

void init_op_table(void) {
    uInt i;

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

void uninit_op_table(void) {
    uInt i;

    for (i = 0; i < NUM_OPERATORS; i++) {
        ident_discard(op_info[i].symbol);
    }
}

Int find_function(const char *name) {
    uInt start = first_function;
    uInt end   = NUM_OPERATORS-1;
    uInt middle = (end+start)/2;

    int rc;

    while ( start <= end &&
            (rc = strcmp(name, op_info[middle].name)) != 0 )
    {
        if (rc < 0)
            end = middle-1;
        else
            start = middle+1;
        middle = (end+start)/2;
    }

    if (start <= end)
        return op_info[middle].opcode;
    else
        return -1;
}

