/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef _moddef_h_
#define _moddef_h_

#include "native.h"
#include "native.h"

#include "cdc.h"
#include "web.h"
#include "ext_math.h"

#define NUM_MODULES 3

#ifdef _native_
module_t * cold_modules[] = {
    &cdc_module,
    &web_module,
    &ext_math_module,
};
#endif

#define NATIVE_BUFFER_LENGTH 0
#define NATIVE_BUFFER_REPLACE 1
#define NATIVE_BUFFER_SUBRANGE 2
#define NATIVE_BUFFER_BUFSUB 3
#define NATIVE_BUFFER_TO_STRING 4
#define NATIVE_BUFFER_TO_STRINGS 5
#define NATIVE_BUFFER_FROM_STRING 6
#define NATIVE_BUFFER_FROM_STRINGS 7
#define NATIVE_DICTIONARY_VALUES 8
#define NATIVE_DICTIONARY_KEYS 9
#define NATIVE_DICTIONARY_ADD 10
#define NATIVE_DICTIONARY_UNION 11
#define NATIVE_DICTIONARY_DEL 12
#define NATIVE_DICTIONARY_CONTAINS 13
#define NATIVE_NETWORK_HOSTNAME 14
#define NATIVE_NETWORK_IP 15
#define NATIVE_LIST_LENGTH 16
#define NATIVE_LIST_SUBRANGE 17
#define NATIVE_LIST_INSERT 18
#define NATIVE_LIST_REPLACE 19
#define NATIVE_LIST_DELETE 20
#define NATIVE_LIST_SETADD 21
#define NATIVE_LIST_SETREMOVE 22
#define NATIVE_LIST_UNION 23
#define NATIVE_LIST_JOIN 24
#define NATIVE_LIST_SORT 25
#define NATIVE_LIST_SORTED_INDEX 26
#define NATIVE_LIST_SORTED_INSERT 27
#define NATIVE_LIST_SORTED_DELETE 28
#define NATIVE_LIST_SORTED_VALIDATE 29
#define NATIVE_STRING_LENGTH 30
#define NATIVE_STRING_SUBRANGE 31
#define NATIVE_STRING_EXPLODE 32
#define NATIVE_STRING_PAD 33
#define NATIVE_STRING_MATCH_BEGIN 34
#define NATIVE_STRING_MATCH_TEMPLATE 35
#define NATIVE_STRING_MATCH_PATTERN 36
#define NATIVE_STRING_MATCH_REGEXP 37
#define NATIVE_STRING_REGEXP 38
#define NATIVE_STRING_SED 39
#define NATIVE_STRING_REPLACE 40
#define NATIVE_STRING_CRYPT 41
#define NATIVE_STRING_UPPERCASE 42
#define NATIVE_STRING_LOWERCASE 43
#define NATIVE_STRING_CAPITALIZE 44
#define NATIVE_STRING_COMPARE 45
#define NATIVE_STRING_FORMAT 46
#define NATIVE_STRING_TRIM 47
#define NATIVE_STRING_SPLIT 48
#define NATIVE_STRING_WORD 49
#define NATIVE_STRING_DBQUOTE_EXPLODE 50
#define NATIVE_SYS_NEXT_OBJNUM 51
#define NATIVE_SYS_STATUS 52
#define NATIVE_SYS_VERSION 53
#define NATIVE_TIME_FORMAT 54
#define NATIVE_INTEGER_AND 55
#define NATIVE_INTEGER_OR 56
#define NATIVE_INTEGER_XOR 57
#define NATIVE_INTEGER_SHLEFT 58
#define NATIVE_INTEGER_SHRIGHT 59
#define NATIVE_INTEGER_NOT 60
#define NATIVE_HTTP_DECODE 61
#define NATIVE_HTTP_ENCODE 62
#define NATIVE_STRING_HTML_ESCAPE 63
#define NATIVE_MATH_MINOR 64
#define NATIVE_MATH_MAJOR 65
#define NATIVE_MATH_ADD 66
#define NATIVE_MATH_SUB 67
#define NATIVE_MATH_DOT 68
#define NATIVE_MATH_DISTANCE 69
#define NATIVE_MATH_CROSS 70
#define NATIVE_MATH_SCALE 71
#define NATIVE_MATH_IS_LOWER 72
#define NATIVE_MATH_TRANSPOSE 73
#define NATIVE_LAST 74

#define MAGIC_MODNUMBER 1083991030


#ifdef _native_
native_t natives[NATIVE_LAST] = {
    {"buffer",       NOT_AN_IDENT, "length",            NOT_AN_IDENT, native_buflen},
    {"buffer",       NOT_AN_IDENT, "replace",           NOT_AN_IDENT, native_buf_replace},
    {"buffer",       NOT_AN_IDENT, "subrange",          NOT_AN_IDENT, native_subbuf},
    {"buffer",       NOT_AN_IDENT, "bufsub",            NOT_AN_IDENT, native_bufsub},
    {"buffer",       NOT_AN_IDENT, "to_string",         NOT_AN_IDENT, native_buf_to_str},
    {"buffer",       NOT_AN_IDENT, "to_strings",        NOT_AN_IDENT, native_buf_to_strings},
    {"buffer",       NOT_AN_IDENT, "from_string",       NOT_AN_IDENT, native_str_to_buf},
    {"buffer",       NOT_AN_IDENT, "from_strings",      NOT_AN_IDENT, native_strings_to_buf},
    {"dictionary",   NOT_AN_IDENT, "values",            NOT_AN_IDENT, native_dict_values},
    {"dictionary",   NOT_AN_IDENT, "keys",              NOT_AN_IDENT, native_dict_keys},
    {"dictionary",   NOT_AN_IDENT, "add",               NOT_AN_IDENT, native_dict_add},
    {"dictionary",   NOT_AN_IDENT, "union",             NOT_AN_IDENT, native_dict_union},
    {"dictionary",   NOT_AN_IDENT, "del",               NOT_AN_IDENT, native_dict_del},
    {"dictionary",   NOT_AN_IDENT, "contains",          NOT_AN_IDENT, native_dict_contains},
    {"network",      NOT_AN_IDENT, "hostname",          NOT_AN_IDENT, native_hostname},
    {"network",      NOT_AN_IDENT, "ip",                NOT_AN_IDENT, native_ip},
    {"list",         NOT_AN_IDENT, "length",            NOT_AN_IDENT, native_listlen},
    {"list",         NOT_AN_IDENT, "subrange",          NOT_AN_IDENT, native_sublist},
    {"list",         NOT_AN_IDENT, "insert",            NOT_AN_IDENT, native_insert},
    {"list",         NOT_AN_IDENT, "replace",           NOT_AN_IDENT, native_replace},
    {"list",         NOT_AN_IDENT, "delete",            NOT_AN_IDENT, native_delete},
    {"list",         NOT_AN_IDENT, "setadd",            NOT_AN_IDENT, native_setadd},
    {"list",         NOT_AN_IDENT, "setremove",         NOT_AN_IDENT, native_setremove},
    {"list",         NOT_AN_IDENT, "union",             NOT_AN_IDENT, native_union},
    {"list",         NOT_AN_IDENT, "join",              NOT_AN_IDENT, native_join},
    {"list",         NOT_AN_IDENT, "sort",              NOT_AN_IDENT, native_sort},
    {"list",         NOT_AN_IDENT, "sorted_index",      NOT_AN_IDENT, native_sorted_index},
    {"list",         NOT_AN_IDENT, "sorted_insert",     NOT_AN_IDENT, native_sorted_insert},
    {"list",         NOT_AN_IDENT, "sorted_delete",     NOT_AN_IDENT, native_sorted_delete},
    {"list",         NOT_AN_IDENT, "sorted_validate",   NOT_AN_IDENT, native_sorted_validate},
    {"string",       NOT_AN_IDENT, "length",            NOT_AN_IDENT, native_strlen},
    {"string",       NOT_AN_IDENT, "subrange",          NOT_AN_IDENT, native_substr},
    {"string",       NOT_AN_IDENT, "explode",           NOT_AN_IDENT, native_explode},
    {"string",       NOT_AN_IDENT, "pad",               NOT_AN_IDENT, native_pad},
    {"string",       NOT_AN_IDENT, "match_begin",       NOT_AN_IDENT, native_match_begin},
    {"string",       NOT_AN_IDENT, "match_template",    NOT_AN_IDENT, native_match_template},
    {"string",       NOT_AN_IDENT, "match_pattern",     NOT_AN_IDENT, native_match_pattern},
    {"string",       NOT_AN_IDENT, "match_regexp",      NOT_AN_IDENT, native_match_regexp},
    {"string",       NOT_AN_IDENT, "regexp",            NOT_AN_IDENT, native_regexp},
    {"string",       NOT_AN_IDENT, "sed",               NOT_AN_IDENT, native_strsed},
    {"string",       NOT_AN_IDENT, "replace",           NOT_AN_IDENT, native_strsub},
    {"string",       NOT_AN_IDENT, "crypt",             NOT_AN_IDENT, native_crypt},
    {"string",       NOT_AN_IDENT, "uppercase",         NOT_AN_IDENT, native_uppercase},
    {"string",       NOT_AN_IDENT, "lowercase",         NOT_AN_IDENT, native_lowercase},
    {"string",       NOT_AN_IDENT, "capitalize",        NOT_AN_IDENT, native_capitalize},
    {"string",       NOT_AN_IDENT, "compare",           NOT_AN_IDENT, native_strcmp},
    {"string",       NOT_AN_IDENT, "format",            NOT_AN_IDENT, native_strfmt},
    {"string",       NOT_AN_IDENT, "trim",              NOT_AN_IDENT, native_trim},
    {"string",       NOT_AN_IDENT, "split",             NOT_AN_IDENT, native_split},
    {"string",       NOT_AN_IDENT, "word",              NOT_AN_IDENT, native_word},
    {"string",       NOT_AN_IDENT, "dbquote_explode",   NOT_AN_IDENT, native_dbquote_explode},
    {"sys",          NOT_AN_IDENT, "next_objnum",       NOT_AN_IDENT, native_next_objnum},
    {"sys",          NOT_AN_IDENT, "status",            NOT_AN_IDENT, native_status},
    {"sys",          NOT_AN_IDENT, "version",           NOT_AN_IDENT, native_version},
    {"time",         NOT_AN_IDENT, "format",            NOT_AN_IDENT, native_strftime},
    {"integer",      NOT_AN_IDENT, "and",               NOT_AN_IDENT, native_and},
    {"integer",      NOT_AN_IDENT, "or",                NOT_AN_IDENT, native_or},
    {"integer",      NOT_AN_IDENT, "xor",               NOT_AN_IDENT, native_xor},
    {"integer",      NOT_AN_IDENT, "shleft",            NOT_AN_IDENT, native_shleft},
    {"integer",      NOT_AN_IDENT, "shright",           NOT_AN_IDENT, native_shright},
    {"integer",      NOT_AN_IDENT, "not",               NOT_AN_IDENT, native_not},
    {"http",         NOT_AN_IDENT, "decode",            NOT_AN_IDENT, native_decode},
    {"http",         NOT_AN_IDENT, "encode",            NOT_AN_IDENT, native_encode},
    {"string",       NOT_AN_IDENT, "html_escape",       NOT_AN_IDENT, native_html_escape},
    {"math",         NOT_AN_IDENT, "minor",             NOT_AN_IDENT, native_minor},
    {"math",         NOT_AN_IDENT, "major",             NOT_AN_IDENT, native_major},
    {"math",         NOT_AN_IDENT, "add",               NOT_AN_IDENT, native_add},
    {"math",         NOT_AN_IDENT, "sub",               NOT_AN_IDENT, native_sub},
    {"math",         NOT_AN_IDENT, "dot",               NOT_AN_IDENT, native_dot},
    {"math",         NOT_AN_IDENT, "distance",          NOT_AN_IDENT, native_distance},
    {"math",         NOT_AN_IDENT, "cross",             NOT_AN_IDENT, native_cross},
    {"math",         NOT_AN_IDENT, "scale",             NOT_AN_IDENT, native_scale},
    {"math",         NOT_AN_IDENT, "is_lower",          NOT_AN_IDENT, native_is_lower},
    {"math",         NOT_AN_IDENT, "transpose",         NOT_AN_IDENT, native_transpose},
};
#else
extern native_t natives[NATIVE_LAST];
#endif

#endif
