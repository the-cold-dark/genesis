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
#define NATIVE_BUFFER_TO_STRING 3
#define NATIVE_BUFFER_TO_STRINGS 4
#define NATIVE_BUFFER_FROM_STRING 5
#define NATIVE_BUFFER_FROM_STRINGS 6
#define NATIVE_DICTIONARY_VALUES 7
#define NATIVE_DICTIONARY_KEYS 8
#define NATIVE_DICTIONARY_ADD 9
#define NATIVE_DICTIONARY_UNION 10
#define NATIVE_DICTIONARY_DEL 11
#define NATIVE_DICTIONARY_CONTAINS 12
#define NATIVE_NETWORK_HOSTNAME 13
#define NATIVE_NETWORK_IP 14
#define NATIVE_LIST_LENGTH 15
#define NATIVE_LIST_SUBRANGE 16
#define NATIVE_LIST_INSERT 17
#define NATIVE_LIST_REPLACE 18
#define NATIVE_LIST_DELETE 19
#define NATIVE_LIST_SETADD 20
#define NATIVE_LIST_SETREMOVE 21
#define NATIVE_LIST_UNION 22
#define NATIVE_LIST_JOIN 23
#define NATIVE_LIST_SORT 24
#define NATIVE_STRING_LENGTH 25
#define NATIVE_STRING_SUBRANGE 26
#define NATIVE_STRING_EXPLODE 27
#define NATIVE_STRING_PAD 28
#define NATIVE_STRING_MATCH_BEGIN 29
#define NATIVE_STRING_MATCH_TEMPLATE 30
#define NATIVE_STRING_MATCH_PATTERN 31
#define NATIVE_STRING_MATCH_REGEXP 32
#define NATIVE_STRING_REGEXP 33
#define NATIVE_STRING_SED 34
#define NATIVE_STRING_REPLACE 35
#define NATIVE_STRING_CRYPT 36
#define NATIVE_STRING_UPPERCASE 37
#define NATIVE_STRING_LOWERCASE 38
#define NATIVE_STRING_CAPITALIZE 39
#define NATIVE_STRING_COMPARE 40
#define NATIVE_STRING_FORMAT 41
#define NATIVE_STRING_TRIM 42
#define NATIVE_STRING_SPLIT 43
#define NATIVE_STRING_WORD 44
#define NATIVE_STRING_DBQUOTE_EXPLODE 45
#define NATIVE_SYS_NEXT_OBJNUM 46
#define NATIVE_SYS_STATUS 47
#define NATIVE_SYS_VERSION 48
#define NATIVE_TIME_FORMAT 49
#define NATIVE_INTEGER_AND 50
#define NATIVE_INTEGER_OR 51
#define NATIVE_INTEGER_XOR 52
#define NATIVE_INTEGER_SHLEFT 53
#define NATIVE_INTEGER_SHRIGHT 54
#define NATIVE_INTEGER_NOT 55
#define NATIVE_HTTP_DECODE 56
#define NATIVE_HTTP_ENCODE 57
#define NATIVE_STRING_HTML_ESCAPE 58
#define NATIVE_MATH_MINOR 59
#define NATIVE_MATH_MAJOR 60
#define NATIVE_MATH_ADD 61
#define NATIVE_MATH_SUB 62
#define NATIVE_MATH_DOT 63
#define NATIVE_MATH_DISTANCE 64
#define NATIVE_MATH_CROSS 65
#define NATIVE_MATH_SCALE 66
#define NATIVE_MATH_IS_LOWER 67
#define NATIVE_MATH_TRANSPOSE 68
#define NATIVE_LAST 69

#define MAGIC_MODNUMBER 0


#ifdef _native_
native_t natives[NATIVE_LAST] = {
    {"buffer",       "length",            native_buflen},
    {"buffer",       "replace",           native_buf_replace},
    {"buffer",       "subrange",          native_subbuf},
    {"buffer",       "to_string",         native_buf_to_str},
    {"buffer",       "to_strings",        native_buf_to_strings},
    {"buffer",       "from_string",       native_str_to_buf},
    {"buffer",       "from_strings",      native_strings_to_buf},
    {"dictionary",   "values",            native_dict_values},
    {"dictionary",   "keys",              native_dict_keys},
    {"dictionary",   "add",               native_dict_add},
    {"dictionary",   "union",             native_dict_union},
    {"dictionary",   "del",               native_dict_del},
    {"dictionary",   "contains",          native_dict_contains},
    {"network",      "hostname",          native_hostname},
    {"network",      "ip",                native_ip},
    {"list",         "length",            native_listlen},
    {"list",         "subrange",          native_sublist},
    {"list",         "insert",            native_insert},
    {"list",         "replace",           native_replace},
    {"list",         "delete",            native_delete},
    {"list",         "setadd",            native_setadd},
    {"list",         "setremove",         native_setremove},
    {"list",         "union",             native_union},
    {"list",         "join",              native_join},
    {"list",         "sort",              native_sort},
    {"string",       "length",            native_strlen},
    {"string",       "subrange",          native_substr},
    {"string",       "explode",           native_explode},
    {"string",       "pad",               native_pad},
    {"string",       "match_begin",       native_match_begin},
    {"string",       "match_template",    native_match_template},
    {"string",       "match_pattern",     native_match_pattern},
    {"string",       "match_regexp",      native_match_regexp},
    {"string",       "regexp",            native_regexp},
    {"string",       "sed",               native_strsed},
    {"string",       "replace",           native_strsub},
    {"string",       "crypt",             native_crypt},
    {"string",       "uppercase",         native_uppercase},
    {"string",       "lowercase",         native_lowercase},
    {"string",       "capitalize",        native_capitalize},
    {"string",       "compare",           native_strcmp},
    {"string",       "format",            native_strfmt},
    {"string",       "trim",              native_trim},
    {"string",       "split",             native_split},
    {"string",       "word",              native_word},
    {"string",       "dbquote_explode",   native_dbquote_explode},
    {"sys",          "next_objnum",       native_next_objnum},
    {"sys",          "status",            native_status},
    {"sys",          "version",           native_version},
    {"time",         "format",            native_strftime},
    {"integer",      "and",               native_and},
    {"integer",      "or",                native_or},
    {"integer",      "xor",               native_xor},
    {"integer",      "shleft",            native_shleft},
    {"integer",      "shright",           native_shright},
    {"integer",      "not",               native_not},
    {"http",         "decode",            native_decode},
    {"http",         "encode",            native_encode},
    {"string",       "html_escape",       native_html_escape},
    {"math",         "minor",             native_minor},
    {"math",         "major",             native_major},
    {"math",         "add",               native_add},
    {"math",         "sub",               native_sub},
    {"math",         "dot",               native_dot},
    {"math",         "distance",          native_distance},
    {"math",         "cross",             native_cross},
    {"math",         "scale",             native_scale},
    {"math",         "is_lower",          native_is_lower},
    {"math",         "transpose",         native_transpose},
};
#else
extern native_t natives[NATIVE_LAST];
#endif

#endif
