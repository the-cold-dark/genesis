/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: opcodes.c
// ---
// Information about opcodes.
*/

#include <ctype.h>
#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "opcodes.h"
#include "operators.h"
#include "old_ops.h"
#include "functions.h"
#include "util.h"

#define NUM_OPERATORS (sizeof(op_info) / sizeof(*op_info))

Op_info op_table[LAST_TOKEN];

static int first_function;

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
    { CATCH,            "CATCH",           op_catch, JUMP, ERROR },
    { CATCH_END,        "CATCH_END",       op_catch_end, JUMP },
    { HANDLER_END,      "HANDLER_END",     op_handler_end },
    { ZERO,             "ZERO",            op_zero },
    { ONE,              "ONE",             op_one },
    { INTEGER,          "INTEGER",         op_integer, INTEGER },
    { FLOAT,            "FLOAT",           op_float, INTEGER },
    { STRING,           "STRING",          op_string, STRING },
    { OBJNUM,           "OBJNUM",          op_objnum, INTEGER },
    { SYMBOL,           "SYMBOL",          op_symbol, IDENT },
    { ERROR,            "ERROR",           op_error, IDENT },
    { NAME,             "NAME",            op_name, IDENT },
    { GET_LOCAL,        "GET_LOCAL",       op_get_local, VAR },
    { GET_OBJ_VAR,      "GET_OBJ_VAR",     op_get_obj_var, IDENT },
    { START_ARGS,       "START_ARGS",      op_start_args },
    { PASS,             "PASS",            op_pass },
    { MESSAGE,          "MESSAGE",         op_message, IDENT },
    { EXPR_MESSAGE,     "EXPR_MESSAGE",    op_expr_message },
    { LIST,             "LIST",            op_list },
    { DICT,             "DICT",            op_dict },
    { BUFFER,           "BUFFER",          op_buffer },
    { FROB,             "FROB",            op_frob },
    { INDEX,            "INDEX",           op_index },
    { AND,              "AND",             op_and, JUMP },
    { OR,               "OR",              op_or, JUMP },
    { CONDITIONAL,      "CONDITIONAL",     op_if, JUMP },
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
    { IN,               "IN",              op_in },

    /* Object variable functions */
    { ADD_VAR,           "add_var",             func_add_var },
    { DEL_VAR,           "del_var",             func_del_var },
    { SET_VAR,           "set_var",             func_set_var },
    { GET_VAR,           "get_var",             func_get_var },
    { CLEAR_VAR,         "clear_var",           func_clear_var },
    { VARIABLES,         "variables",           func_variables },

    /* Object method functions */
    { COMPILE,           "compile",             func_null_function },
    { DECOMPILE,         "decompile",           func_decompile },
    { ADD_METHOD,        "add_method",          func_add_method },
    { DEL_METHOD,        "del_method",          func_del_method },
    { GET_METHOD,        "get_method",          func_null_function },
    { RENAME_METHOD,     "rename_method",       func_rename_method },
    { SET_METHOD_FLAGS,  "set_method_flags",    func_set_method_flags },
    { SET_METHOD_ACCESS, "set_method_access",   func_set_method_access },
    { METHOD_INFO,       "method_info",         func_method_info },
    { METHOD_FLAGS,      "method_flags",        func_method_flags },
    { METHOD_ACCESS,     "method_access",       func_method_access },
    { METHODS,           "methods",             func_methods },
    { FIND_METHOD,       "find_method",         func_find_method },
    { FIND_NEXT_METHOD,  "find_next_method",    func_find_next_method },
    
    /* Object functions */
    { PARENTS,           "parents",             func_parents },
    { CHILDREN,          "children",            func_children },
    { ANCESTORS,         "ancestors",           func_ancestors },
    { HAS_ANCESTOR,      "has_ancestor",        func_has_ancestor },
    { DESCENDANTS,       "descendants",         func_descendants },
    { SIZE,              "size",                func_size },
    { CREATE,            "create",              func_create },
    { CHPARENTS,         "chparents",           func_chparents },
    { DESTROY,           "destroy",             func_destroy },
    { SET_OBJNAME,       "set_objname",         func_set_objname },
    { DEL_OBJNAME,       "del_objname",         func_del_objname },
    { OBJNAME,           "objname",             func_objname },
    { F_OBJNUM,          "objnum",              func_objnum },
    { LOOKUP,            "lookup",              func_lookup },
    { DATA,              "data",                func_data },

    /* System functions */
    { LOG,               "log",                 func_log },
    { BACKUP,            "backup",              func_backup },
    { SHUTDOWN,          "shutdown",            func_shutdown },
    { SET_HEARTBEAT,     "set_heartbeat",       func_set_heartbeat },

    /* Task/Frame functions */
    { TICK,             "tick",                 func_tick },
    { RESUME,           "resume",               func_resume },
    { SUSPEND,          "suspend",              func_suspend },
    { TASKS,            "tasks",                func_tasks },
    { TASK_ID,          "task_id",              func_task_id },
    { CANCEL,           "cancel",               func_cancel },
    { PAUSE,            "pause",                func_pause },
    { REFRESH,          "refresh",              func_refresh },
    { TICKS_LEFT,       "ticks_left",           func_ticks_left },
    { METHOD,           "method",               func_method },
    { THIS,             "this",                 func_this },
    { DEFINER,          "definer",              func_definer },
    { SENDER,           "sender",               func_sender },
    { CALLER,           "caller",               func_caller },
    { STACK,            "stack",                func_stack },
    { ATOMIC,           "atomic",               func_atomic },

    /* Data/Conversion functions */
    { VALID,            "valid",                func_valid },
    { TYPE,             "type",                 func_type },
    { CLASS,            "class",                func_class },
    { TOINT,            "toint",                func_toint },
    { TOFLOAT,          "tofloat",              func_tofloat },
    { TOSTR,            "tostr",                func_tostr },
    { TOLITERAL,        "toliteral",            func_toliteral },
    { TOOBJNUM,         "toobjnum",             func_toobjnum },
    { TOSYM,            "tosym",                func_tosym },
    { TOERR,            "toerr",                func_toerr },

    /* Exception functions */
    { ERROR_FUNC,       "error",                func_error },
    { TRACEBACK,        "traceback",            func_traceback },
    { THROW,            "throw",                func_throw },
    { RETHROW,          "rethrow",              func_rethrow },

    /* Network control functions */
    { REASSIGN_CONNECTION,"reassign_connection",func_reassign_connection },
    { BIND_PORT,        "bind_port",            func_bind_port },
    { UNBIND_PORT,      "unbind_port",          func_unbind_port },
    { OPEN_CONNECTION,  "open_connection",      func_open_connection },
    { CLOSE_CONNECTION, "close_connection",     func_close_connection },
    { CWRITE,           "cwrite",               func_cwrite },
    { CWRITEF,          "cwritef",              func_cwritef },
    { CONNECTION,       "connection",           func_connection },

    /* File control functions */
    { EXECUTE,          "execute",              func_execute },
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
    { LOCALTIME,        "localtime",       op_localtime },
    { TIME,             "time",            op_time },
    { MTIME,            "mtime",           op_mtime },
    { CTIME,            "ctime",           op_ctime },
    { BIND_FUNCTION,    "bind_function",   func_bind_function },
    { UNBIND_FUNCTION,  "unbind_function", func_unbind_function },
    { RANDOM,           "random",          op_random },
    { F_MIN,            "min",             op_min },
    { F_MAX,            "max",             op_max },
    { F_ABS,            "abs",             op_abs },

    /* -------- from here on are native functions -------- */
    /* Operations on strings (stringop.c). */
    { STRFMT,           "strfmt",          op_strfmt },
    { STRLEN,           "strlen",          op_strlen },
    { SUBSTR,           "substr",          op_substr },
    { EXPLODE,          "explode",         op_explode },
    { STRSUB,           "strsub",          op_strsub },
    { PAD,              "pad",             op_pad },
    { MATCH_BEGIN,      "match_begin",     op_match_begin },
    { MATCH_TEMPLATE,   "match_template",  op_match_template },
    { MATCH_PATTERN,    "match_pattern",   op_match_pattern },
    { MATCH_REGEXP,     "match_regexp",    op_match_regexp },
    { CRYPT,            "crypt",           op_crypt },
    { UPPERCASE,        "uppercase",       op_uppercase },
    { LOWERCASE,        "lowercase",       op_lowercase },
    { STRCMP,           "strcmp",          op_strcmp },

    /* List manipulation (listop.c). */
    { LISTLEN,          "listlen",         op_listlen },
    { SUBLIST,          "sublist",         op_sublist },
    { INSERT,           "insert",          op_insert },
    { REPLACE,          "replace",         op_replace },
    { DELETE,           "delete",          op_delete },
    { SETADD,           "setadd",          op_setadd },
    { SETREMOVE,        "setremove",       op_setremove },
    { UNION,            "union",           op_union },

    /* Dictionary manipulation (dictop.c). */
    { DICT_KEYS,        "dict_keys",       op_dict_keys },
    { DICT_ADD,         "dict_add",        op_dict_add },
    { DICT_DEL,         "dict_del",        op_dict_del },
    { DICT_CONTAINS,    "dict_contains",   op_dict_contains },

    /* Buffer manipulation (bufferop.c). */
    { BUFFER_LEN,       "buffer_len",      op_buffer_len },
    { BUFFER_RETRIEVE,  "buffer_retrieve", op_buffer_retrieve },
    { BUFFER_APPEND,    "buffer_append",   op_buffer_append },
    { BUFFER_REPLACE,   "buffer_replace",  op_buffer_replace },
    { BUFFER_ADD,       "buffer_add",      op_buffer_add },
    { BUFFER_TRUNCATE,  "buffer_truncate", op_buffer_truncate },
    { BUFFER_TAIL,      "buffer_tail",     op_buffer_tail },
    { BUFFER_TO_STRINGS,"buffer_to_strings",op_buffer_to_strings },
    { BUFFER_TO_STRING,"buffer_to_string",op_buffer_to_string },
    { BUFFER_FROM_STRINGS,"buffer_from_strings",op_buffer_from_strings },
    { BUFFER_FROM_STRING,"buffer_from_string",op_buffer_from_string },

    { HOSTNAME,         "hostname",             op_hostname },
    { IP,               "ip",                   op_ip },
    { STATUS,           "status",                op_status },
    { NEXT_OBJNUM,      "next_objnum",                op_next_objnum },
    { VERSION,          "version",         op_version },
    { STRFTIME,         "strftime",        op_strftime },
    { TOKENIZE_CML,     "tokenize_cml",    op_tokenize_cml },
    { BUF_TO_VEIL_PACKETS,      "buf_to_veil_packets",   op_buf_to_veil_packets },
    { BUF_FROM_VEIL_PACKETS,    "buf_from_veil_packets", op_buf_from_veil_packets },

    { DECODE, "decode", native_decode },
    { ENCODE, "encode", native_encode }
};

void init_op_table(void) {
    int i;

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

int find_function(char *name) {
    int i;

    for (i = first_function; i < NUM_OPERATORS; i++) {
        if (strcmp(op_info[i].name, name) == 0)
            return op_info[i].opcode;
    }

    return -1;
}

