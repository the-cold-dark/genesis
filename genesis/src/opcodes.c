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
    { DBREF,            "DBREF",           op_dbref, INTEGER },
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
#if 0
    { '&',              "&",               op_bwand },
    { '|',              "|",               op_bwor },
    { SR,               ">>",              op_bwshr },
    { SL,               "<<",              op_bwshl },
#endif

    { METHOD,           "method",          func_method },
    { THIS,             "this",            func_this },
    { DEFINER,          "definer",         func_definer },
    { SENDER,           "sender",          func_sender },
    { CALLER,           "caller",          func_caller },
    { TASK_ID,          "task_id",         func_task_id },

    /* Generic data manipulation (dataop.c). */
    { TYPE,             "type",            func_type },
    { CLASS,            "class",           func_class },
    { TOINT,            "toint",           func_toint },
    { TOFLOAT,          "tofloat",         func_tofloat },
    { TOSTR,            "tostr",           func_tostr },
    { TOLITERAL,        "toliteral",       func_toliteral },
    { TODBREF,          "todbref",         func_todbref },
    { TOSYM,            "tosym",           func_tosym },
    { TOERR,            "toerr",           func_toerr },
    { VALID,            "valid",           func_valid },

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

    { VERSION,          "version",         op_version },
    { RANDOM,           "random",          op_random },
    { TIME,             "time",            op_time },
    { LOCALTIME,        "localtime",       op_localtime },
    { MTIME,            "mtime",           op_mtime },
    { TIMESTAMP,        "timestamp",       op_timestamp },
    { STRFTIME,         "strftime",        op_strftime },
    { CTIME,            "ctime",           op_ctime },
    { MIN,              "min",             op_min },
    { MAX,              "max",             op_max },
    { ABS,              "abs",             op_abs },
    { GET_DBREF,        "get_dbref",       op_get_dbref },
    { TICKS_LEFT,       "ticks_left",      func_ticks_left },
    { TOKENIZE_CML,     "tokenize_cml",    op_tokenize_cml },
    { BUF_TO_VEIL_PACKETS,      "buf_to_veil_packets",   op_buf_to_veil_packets },
    { BUF_FROM_VEIL_PACKETS,    "buf_from_veil_packets", op_buf_from_veil_packets },

    { ERROR_FUNC,       "error",           func_error },
    { TRACEBACK,        "traceback",       func_traceback },
    { THROW,            "throw",           func_throw },
    { RETHROW,          "rethrow",         func_rethrow },

    /* Input and output (ioop.c). */
    { ECHO_FUNC,        "echo",                        op_echo },
    { ECHO_FILE,        "echo_file",                op_echo_file },
    { STAT_FILE,        "stat_file",                op_stat_file },
    { READ_FILE,        "read_file",                op_read_file },
    { CLOSE_CONNECTION,        "close_connection",        op_close_connection },

    /* Operations on the current object (objectop.c). */
    { ADD_PARAMETER,        "add_parameter",        func_add_parameter },
    { PARAMETERS,        "parameters",                func_parameters },
    { DEL_PARAMETER,        "del_parameter",        func_del_parameter },
    { SET_VAR,                "set_var",                func_set_var },
    { GET_VAR,                "get_var",                func_get_var },
    { CLEAR_VAR,        "clear_var",                func_clear_var },
    { COMPILE,                "compile",                func_compile_to_method },
    { SET_METHOD_FLAGS,     "set_method_flags",    func_set_method_flags },
    { SET_METHOD_STATE,     "set_method_state",    func_set_method_state },
    { METHOD_ARGS,          "method_args",    func_method_args },
    { METHOD_FLAGS,         "method_flags",        func_method_flags },
    { METHOD_STATE,         "method_state",        func_method_state },
    { METHODS,                "methods",                func_methods },
    { FIND_METHOD,        "find_method",                func_find_method },
    { FIND_NEXT_METHOD,        "find_next_method",        func_find_next_method },
    { LIST_METHOD,        "list_method",                func_list_method },
    { DEL_METHOD,        "del_method",                func_del_method },
    { PARENTS,                "parents",                func_parents },
    { CHILDREN,                "children",                func_children },
    { ANCESTORS,        "ancestors",                func_ancestors },
    { HAS_ANCESTOR,        "has_ancestor",                func_has_ancestor },
    { SIZE,                "size",                        func_size },

    /* administrative / system operations (sysop.c). */
    { CREATE,                "create",                func_create },
    { CHPARENTS,        "chparents",                func_chparents },
    { DESTROY,                "destroy",                func_destroy },
    { LOG,                "log",                        func_log },
    { BACKUP,                "backup",                func_backup },
    { BINARY_DUMP,        "binary_dump",                func_binary_dump },
    { TEXT_DUMP,        "text_dump",                func_text_dump },
    { EXECUTE,                "execute",                func_execute },
    { SHUTDOWN,                "shutdown",                func_shutdown },
    { SET_HEARTBEAT,        "set_heartbeat",        func_set_heartbeat },
    { DATA,                "data",                        func_data },
    { ADD_OBJNAME,        "add_objname",                func_add_objname },
    { DEL_OBJNAME,        "del_objname",                func_del_objname },
    { TICK,                "tick",                        func_tick },
    { RESUME,           "resume",               func_resume },
    { SUSPEND,          "suspend",              func_suspend },
    { TASKS,            "tasks",                func_tasks },
    { CANCEL,           "cancel",               func_cancel },
    { PAUSE,            "pause",                func_pause },
    { CALLERS,          "callers",              func_callers },
    { LOAD,                "load",                        op_load },
    { STATUS,                "status",                op_status },
    { BIND_FUNCTION,        "bind_function",        func_bind_function },
    { UNBIND_FUNCTION,        "unbind_function",        func_unbind_function },

    { NEXT_DBREF,        "next_dbref",                op_next_dbref },
    { HOSTNAME,                "hostname",                op_hostname },
    { IP,                "ip",                        op_ip },
    { BIND_PORT,        "bind_port",                op_bind_port },
    { UNBIND_PORT,        "unbind_port",                op_unbind_port },
    { OPEN_CONNECTION,        "open_connection",        op_open_connection },
    { REASSIGN_CONNECTION, "reassign_connection", op_reassign_connection }
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

