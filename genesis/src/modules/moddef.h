/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef _moddef_h_
#define _moddef_h_

#include "native.h"
#include "native.h"

#include "cdc.h"
#include "veil.h"
#include "web.h"

#define NUM_MODULES 3

#ifdef _native_
module_t * cold_modules[] = {
    &cdc_module,
    &veil_module,
    &web_module,
};
#endif

#define NATIVE_BUFFER_LENGTH 0
#define NATIVE_BUFFER_REPLACE 1
#define NATIVE_BUFFER_SUBRANGE 2
#define NATIVE_BUFFER_TO_STRING 3
#define NATIVE_BUFFER_TO_STRINGS 4
#define NATIVE_BUFFER_FROM_STRING 5
#define NATIVE_BUFFER_FROM_STRINGS 6
#define NATIVE_DICTIONARY_KEYS 7
#define NATIVE_DICTIONARY_ADD 8
#define NATIVE_DICTIONARY_DEL 9
#define NATIVE_DICTIONARY_CONTAINS 10
#define NATIVE_NETWORK_HOSTNAME 11
#define NATIVE_NETWORK_IP 12
#define NATIVE_LIST_LENGTH 13
#define NATIVE_LIST_SUBRANGE 14
#define NATIVE_LIST_INSERT 15
#define NATIVE_LIST_REPLACE 16
#define NATIVE_LIST_DELETE 17
#define NATIVE_LIST_SETADD 18
#define NATIVE_LIST_SETREMOVE 19
#define NATIVE_LIST_UNION 20
#define NATIVE_LIST_JOIN 21
#define NATIVE_STRING_LENGTH 22
#define NATIVE_STRING_SUBRANGE 23
#define NATIVE_STRING_EXPLODE 24
#define NATIVE_STRING_PAD 25
#define NATIVE_STRING_MATCH_BEGIN 26
#define NATIVE_STRING_MATCH_TEMPLATE 27
#define NATIVE_STRING_MATCH_PATTERN 28
#define NATIVE_STRING_MATCH_REGEXP 29
#define NATIVE_STRING_REGEXP 30
#define NATIVE_STRING_SED 31
#define NATIVE_STRING_REPLACE 32
#define NATIVE_STRING_CRYPT 33
#define NATIVE_STRING_UPPERCASE 34
#define NATIVE_STRING_LOWERCASE 35
#define NATIVE_STRING_CAPITALIZE 36
#define NATIVE_STRING_COMPARE 37
#define NATIVE_STRING_FORMAT 38
#define NATIVE_STRING_TRIM 39
#define NATIVE_STRING_SPLIT 40
#define NATIVE_SYS_NEXT_OBJNUM 41
#define NATIVE_SYS_STATUS 42
#define NATIVE_SYS_VERSION 43
#define NATIVE_TIME_FORMAT 44
#define NATIVE_BUFFER_TO_VEIL_PKTS 45
#define NATIVE_BUFFER_FROM_VEIL_PKTS 46
#define NATIVE_HTTP_DECODE 47
#define NATIVE_HTTP_ENCODE 48
#define NATIVE_LAST 49

#define MAGIC_MODNUMBER 836857006


#ifdef _native_
native_t natives[NATIVE_LAST] = {
    {"buffer",       "length",            native_buflen},
    {"buffer",       "replace",           native_buf_replace},
    {"buffer",       "subrange",          native_subbuf},
    {"buffer",       "to_string",         native_buf_to_str},
    {"buffer",       "to_strings",        native_buf_to_strings},
    {"buffer",       "from_string",       native_str_to_buf},
    {"buffer",       "from_strings",      native_strings_to_buf},
    {"dictionary",   "keys",              native_dict_keys},
    {"dictionary",   "add",               native_dict_add},
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
    {"sys",          "next_objnum",       native_next_objnum},
    {"sys",          "status",            native_status},
    {"sys",          "version",           native_version},
    {"time",         "format",            native_strftime},
    {"buffer",       "to_veil_pkts",      native_to_veil_pkts},
    {"buffer",       "from_veil_pkts",    native_from_veil_pkts},
    {"http",         "decode",            native_decode},
    {"http",         "encode",            native_encode},
};
#else
extern native_t natives[NATIVE_LAST];
#endif

#endif
