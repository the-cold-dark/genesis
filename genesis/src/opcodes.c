/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include <ctype.h>
#include "cdc_pcode.h"
#include "operators.h"
#include "functions.h"
#include "util.h"

#define FDEF(_def_, _str_, _name_)  { _def_, _str_, CAT(func_, _name_) }

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

#ifdef USE_BIG_FLOATS
    /* Big floats are the size of two ints */
    { FLOAT,		"FLOAT",	   op_float, INTEGER, INTEGER },
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

    /* Object variable functions, MUST be in alpha order */
    FDEF(F_ABS,                   "abs",                   abs),
    FDEF(F_ACOS,                  "acos",                  acos),
    FDEF(F_ADD_METHOD,            "add_method",            add_method),
    FDEF(F_ADD_VAR,               "add_var",               add_var),
    FDEF(F_ANCESTORS,             "ancestors",             ancestors),
    FDEF(F_ANTICIPATE_ASSIGNMENT, "anticipate_assignment", anticipate_assignment),
    FDEF(F_ASIN,                  "asin",                  asin),
    FDEF(F_ATAN,                  "atan",                  atan),
    FDEF(F_ATAN2,                 "atan2",                 atan2),
    FDEF(F_ATOMIC,                "atomic",                atomic),
    FDEF(F_BACKUP,                "backup",                backup),
    FDEF(F_BIND_FUNCTION,         "bind_function",         bind_function),
    FDEF(F_BIND_PORT,             "bind_port",             bind_port),
    FDEF(F_BUF_REPLACE,           "buf_replace",           buf_replace),
    FDEF(F_BUF_TO_STR,            "buf_to_str",            buf_to_str),
    FDEF(F_BUF_TO_STRINGS,        "buf_to_strings",        buf_to_strings),
    FDEF(F_BUFGRAFT,              "bufgraft",              bufgraft),
    FDEF(F_BUFIDX,                "bufidx",                bufidx),
    FDEF(F_BUFLEN,                "buflen",                buflen),
    FDEF(F_BUFSUB,                "bufsub",                bufsub),
    FDEF(F_CACHE_INFO,            "cache_info",            cache_info),
    FDEF(F_CACHE_STATS,           "cache_stats",           cache_stats),
    FDEF(F_CALL_TRACE,            "call_trace",            call_trace),
    FDEF(F_CALLER,                "caller",                caller),
    FDEF(F_CALLING_METHOD,        "calling_method",        calling_method),
    FDEF(F_CANCEL,                "cancel",                cancel),
    FDEF(F_CHILDREN,              "children",              children),
    FDEF(F_CHPARENTS,             "chparents",             chparents),
    FDEF(F_CLASS,                 "class",                 frob_class),   /* devalued, see frob_class */
    FDEF(F_CLEAR_VAR,             "clear_var",             clear_var),
    FDEF(F_CLOSE_CONNECTION,      "close_connection",      close_connection),
    FDEF(F_CONFIG,                "config",                config),
    FDEF(F_CONNECTION,            "connection",            connection),
    FDEF(F_COS,                   "cos",                   cos),
    FDEF(F_CREATE,                "create",                create),
    FDEF(F_CRYPT,                 "crypt",                 crypt),
    FDEF(F_CTIME,                 "ctime",                 ctime),
    FDEF(F_CWRITE,                "cwrite",                cwrite),
    FDEF(F_CWRITEF,               "cwritef",               cwritef),
    FDEF(F_DATA,                  "data",                  data),
    FDEF(F_DBLOG,                 "dblog",                 dblog),
    FDEF(F_DEBUG_CALLERS,         "debug_callers",         debug_callers),
    FDEF(F_DEFAULT_VAR,           "default_var",           default_var),
    FDEF(F_DEFINER,               "definer",               definer),
    FDEF(F_DEL_METHOD,            "del_method",            del_method),
    FDEF(F_DEL_OBJNAME,           "del_objname",           del_objname),
    FDEF(F_DEL_VAR,               "del_var",               del_var),
    FDEF(F_DELETE,                "delete",                delete),
    FDEF(F_DESTROY,               "destroy",               destroy),
    FDEF(F_DICT_ADD,              "dict_add",              dict_add),
    FDEF(F_DICT_CONTAINS,         "dict_contains",         dict_contains),
    FDEF(F_DICT_DEL,              "dict_del",              dict_del),
    FDEF(F_DICT_KEYS,             "dict_keys",             dict_keys),
    FDEF(F_DICT_UNION,            "dict_union",            dict_union),
    FDEF(F_DICT_VALUES,           "dict_values",           dict_values),
    FDEF(F_ERROR_FUNC,            "error",                 error),
    FDEF(F_ERROR_DATA,            "error_data",            error_data),
    FDEF(F_ERROR_MESSAGE,         "error_message",         error_message),
    FDEF(F_EXECUTE,               "execute",               execute),
    FDEF(F_EXP,                   "exp",                   exp),
    FDEF(F_EXPLODE,               "explode",               explode),
    FDEF(F_EXPLODE_QUOTED,        "explode_quoted",        explode_quoted),
    FDEF(F_FCHMOD,                "fchmod",                fchmod),
    FDEF(F_FCLOSE,                "fclose",                fclose),
    FDEF(F_FEOF,                  "feof",                  feof),
    FDEF(F_FFLUSH,                "fflush",                fflush),
    FDEF(F_FILE,                  "file",                  file),
    FDEF(F_FILES,                 "files",                 files),
    FDEF(F_FIND_METHOD,           "find_method",           find_method),
    FDEF(F_FIND_NEXT_METHOD,      "find_next_method",      find_next_method),
    FDEF(F_FMKDIR,                "fmkdir",                fmkdir),
    FDEF(F_FOPEN,                 "fopen",                 fopen),
    FDEF(F_FREAD,                 "fread",                 fread),
    FDEF(F_FREMOVE,               "fremove",               fremove),
    FDEF(F_FRENAME,               "frename",               frename),
    FDEF(F_FRMDIR,                "frmdir",                frmdir),
    FDEF(F_FROB_CLASS,            "frob_class",            frob_class),
    FDEF(F_FROB_HANDLER,          "frob_handler",          frob_handler),
    FDEF(F_FROB_VALUE,            "frob_value",            frob_value),
    FDEF(F_FROMLITERAL,           "fromliteral",           fromliteral),
    FDEF(F_FSEEK,                 "fseek",                 fseek),
    FDEF(F_FSTAT,                 "fstat",                 fstat),
    FDEF(F_FWRITE,                "fwrite",                fwrite),
    FDEF(F_GET_VAR,               "get_var",               get_var),
    FDEF(F_HAS_ANCESTOR,          "has_ancestor",          has_ancestor),
    FDEF(F_HAS_METHOD,            "has_method",            has_method),
    FDEF(F_INHERITED_VAR,         "inherited_var",         inherited_var),
    FDEF(F_INSERT,                "insert",                insert),
    FDEF(F_JOIN,                  "join",                  join),
    FDEF(F_LIST_METHOD,           "list_method",           list_method),
    FDEF(F_LISTGRAFT,             "listgraft",             listgraft),
    FDEF(F_LISTIDX,               "listidx",               listidx),
    FDEF(F_LISTLEN,               "listlen",               listlen),
    FDEF(F_LOCALTIME,             "localtime",             localtime),
    FDEF(F_LOG,                   "log",                   log),
    FDEF(F_LOOKUP,                "lookup",                lookup),
    FDEF(F_LOWERCASE,             "lowercase",             lowercase),
    FDEF(F_MATCH_BEGIN,           "match_begin",           match_begin),
    FDEF(F_MATCH_CRYPTED,         "match_crypted",         match_crypted),
    FDEF(F_MATCH_PATTERN,         "match_pattern",         match_pattern),
    FDEF(F_MATCH_REGEXP,          "match_regexp",          match_regexp),
    FDEF(F_MATCH_TEMPLATE,        "match_template",        match_template),
    FDEF(F_MAX,                   "max",                   max),
    FDEF(F_MEMORY_SIZE,           "memory_size",           memory_size),
    FDEF(F_METHODOP,              "method",                method),
    FDEF(F_METHOD_ACCESS,         "method_access",         method_access),
    FDEF(F_METHOD_BYTECODE,       "method_bytecode",       method_bytecode),
    FDEF(F_METHOD_FLAGS,          "method_flags",          method_flags),
    FDEF(F_METHOD_INFO,           "method_info",           method_info),
    FDEF(F_METHODS,               "methods",               methods),
    FDEF(F_MIN,                   "min",                   min),
    FDEF(F_MTIME,                 "mtime",                 mtime),
    FDEF(F_OBJNAME,               "objname",               objname),
    FDEF(F_OBJNUM,                "objnum",                objnum),
    FDEF(F_OPEN_CONNECTION,       "open_connection",       open_connection),
    FDEF(F_PAD,                   "pad",                   pad),
    FDEF(F_PARENTS,               "parents",               parents),
    FDEF(F_PAUSE,                 "pause",                 pause),
    FDEF(F_POW,                   "pow",                   pow),
    FDEF(F_RANDOM,                "random",                random),
    FDEF(F_REASSIGN_CONNECTION,   "reassign_connection",   reassign_connection),
    FDEF(F_REFRESH,               "refresh",               refresh),
    FDEF(F_REGEXP,                "regexp",                regexp),
    FDEF(F_RENAME_METHOD,         "rename_method",         rename_method),
    FDEF(F_REPLACE,               "replace",               replace),
    FDEF(F_RESUME,                "resume",                resume),
    FDEF(F_RETHROW,               "rethrow",               rethrow),
    FDEF(F_ROUND,                 "round",                 round),
    FDEF(F_SENDER,                "sender",                sender),
    FDEF(F_SET_HEARTBEAT,         "set_heartbeat",         set_heartbeat),
    FDEF(F_SET_METHOD_ACCESS,     "set_method_access",     set_method_access),
    FDEF(F_SET_METHOD_FLAGS,      "set_method_flags",      set_method_flags),
    FDEF(F_SET_OBJNAME,           "set_objname",           set_objname),
    FDEF(F_SET_USER,              "set_user",              set_user),
    FDEF(F_SET_VAR,               "set_var",               set_var),
    FDEF(F_SETADD,                "setadd",                setadd),
    FDEF(F_SETREMOVE,             "setremove",             setremove),
    FDEF(F_SHUTDOWN,              "shutdown",              shutdown),
    FDEF(F_SIN,                   "sin",                   sin),
    FDEF(F_SIZE,                  "size",                  size),
    FDEF(F_SPLIT,                 "split",                 split),
    FDEF(F_SQRT,                  "sqrt",                  sqrt),
    FDEF(F_STACK,                 "stack",                 stack),
    FDEF(F_STR_TO_BUF,            "str_to_buf",            str_to_buf),
    FDEF(F_STRCMP,                "strcmp",                strcmp),
    FDEF(F_STRFMT,                "strfmt",                strfmt),
    FDEF(F_STRGRAFT,              "strgraft",              strgraft),
    FDEF(F_STRIDX,                "stridx",                stridx),
    FDEF(F_STRINGS_TO_BUF,        "strings_to_buf",        strings_to_buf),
    FDEF(F_STRLEN,                "strlen",                strlen),
    FDEF(F_STRSED,                "strsed",                strsed),
    FDEF(F_STRSUB,                "strsub",                strsub),
    FDEF(F_SUBBUF,                "subbuf",                subbuf),
    FDEF(F_SUBLIST,               "sublist",               sublist),
    FDEF(F_SUBSTR,                "substr",                substr),
    FDEF(F_SUSPEND,               "suspend",               suspend),
    FDEF(F_SYNC,                  "sync",                  sync),
    FDEF(F_TAN,                   "tan",                   tan),
    FDEF(F_TASK_ID,               "task_id",               task_id),
    FDEF(F_TASK_INFO,             "task_info",             task_info),
    FDEF(F_TASKS,                 "tasks",                 tasks),
    FDEF(F_THIS,                  "this",                  this),
    FDEF(F_THROW,                 "throw",                 throw),
    FDEF(F_TICK,                  "tick",                  tick),
    FDEF(F_TICKS_LEFT,            "ticks_left",            ticks_left),
    FDEF(F_TIME,                  "time",                  time),
    FDEF(F_TOERR,                 "toerr",                 toerr),
    FDEF(F_TOFLOAT,               "tofloat",               tofloat),
    FDEF(F_TOINT,                 "toint",                 toint),
    FDEF(F_TOLITERAL,             "toliteral",             toliteral),
    FDEF(F_TOOBJNUM,              "toobjnum",              toobjnum),
    FDEF(F_TOSTR,                 "tostr",                 tostr),
    FDEF(F_TOSYM,                 "tosym",                 tosym),
    FDEF(F_TRACEBACK,             "traceback",             traceback),
    FDEF(F_TYPE,                  "type",                  type),
    FDEF(F_UNBIND_FUNCTION,       "unbind_function",       unbind_function),
    FDEF(F_UNBIND_PORT,           "unbind_port",           unbind_port),
    FDEF(F_UNION,                 "union",                 union),
    FDEF(F_UPPERCASE,             "uppercase",             uppercase),
    FDEF(F_USER,                  "user",                  user),
    FDEF(F_VALID,                 "valid",                 valid),
    FDEF(F_VARIABLES,             "variables",             variables)
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

Int find_function(char *name) {
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

