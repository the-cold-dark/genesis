/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/moddef.h
// ---
//
*/

#ifndef _moddef_h_
#define _moddef_h_

#include "modules.h"
#include "native.h"

#include "cdc.h"
#include "core.h"
#include "web.h"

#define MAGIC_MODNUMBER 826426044

module_t * cold_modules[] = {
    &cdc_module,
    &core_module,
    &web_module,
};

#define NATIVE_BUFFER_LENGTH 0
#define NATIVE_BUFFER_RETRIEVE 1
#define NATIVE_BUFFER_APPEND 2
#define NATIVE_BUFFER_REPLACE 3
#define NATIVE_BUFFER_SUBRANGE 4
#define NATIVE_BUFFER_ADD 5
#define NATIVE_BUFFER_TRUNCATE 6
#define NATIVE_BUFFER_TO_STRING 7
#define NATIVE_BUFFER_TO_STRINGS 8
#define NATIVE_BUFFER_FROM_STRING 9
#define NATIVE_BUFFER_FROM_STRINGS 10
#define NATIVE_DICTIONARY_KEYS 11
#define NATIVE_DICTIONARY_ADD 12
#define NATIVE_DICTIONARY_DEL 13
#define NATIVE_DICTIONARY_CONTAINS 14
#define NATIVE_NETWORK_HOSTNAME 15
#define NATIVE_NETWORK_IP 16
#define NATIVE_LIST_LENGTH 17
#define NATIVE_LIST_SUBRANGE 18
#define NATIVE_LIST_INSERT 19
#define NATIVE_LIST_REPLACE 20
#define NATIVE_LIST_DELETE 21
#define NATIVE_LIST_SETADD 22
#define NATIVE_LIST_SETREMOVE 23
#define NATIVE_LIST_UNION 24
#define NATIVE_STRING_LENGTH 25
#define NATIVE_STRING_SUBRANGE 26
#define NATIVE_STRING_EXPLODE 27
#define NATIVE_STRING_PAD 28
#define NATIVE_STRING_MATCH_BEGIN 29
#define NATIVE_STRING_MATCH_TEMPLATE 30
#define NATIVE_STRING_MATCH_PATTERN 31
#define NATIVE_STRING_MATCH_REGEXP 32
#define NATIVE_STRING_SED 33
#define NATIVE_STRING_REPLACE 34
#define NATIVE_STRING_CRYPT 35
#define NATIVE_STRING_UPPERCASE 36
#define NATIVE_STRING_LOWERCASE 37
#define NATIVE_STRING_COMPARE 38
#define NATIVE_STRING_FORMAT 39
#define NATIVE_SYS_NEXT_OBJNUM 40
#define NATIVE_SYS_STATUS 41
#define NATIVE_SYS_VERSION 42
#define NATIVE_TIME_FORMAT 43
#define NATIVE_BUFFER_TO_VEIL_PACKETS 44
#define NATIVE_BUFFER_FROM_VEIL_PACKETS 45
#define NATIVE_HTTP_DECODE 46
#define NATIVE_HTTP_ENCODE 47

native_t natives[] = {
};

#endif
