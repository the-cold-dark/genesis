/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/cdc_types.h
// ---
//
*/

#ifndef _cdc_types_h_
#define _cdc_types_h_

typedef struct string       string_t;
typedef struct list         list_t;
typedef struct buffer       Buffer;
typedef struct frob         Frob;
typedef struct data         data_t;
typedef struct dict         Dict;
typedef        long         Ident;
typedef        long         Dbref;
typedef struct ident_entry  Ident_entry;
typedef struct string_entry String_entry;
typedef struct var          Var;
typedef struct object       object_t;
typedef struct method       method_t;
typedef struct error_list   Error_list;
typedef int                 Object_string;
typedef int                 Object_ident;

#include "regexp.h"

struct string {
    int start;
    int len;
    int size;
    int refs;
    regexp * reg;
    char s[1];
};

struct buffer {
    int len;
    int refs;
    unsigned char s[1];
};

struct data {
    int type;
    union {
        long val;
        float fval;
        Dbref dbref;
        Ident symbol;
        Ident error;
        string_t * str;
        list_t * list;
        Frob * frob;
        Dict * dict;
        Buffer * buffer;
    } u;
};

struct list {
    int start;
    int len;
    int size;
    int refs;
    data_t el[1];
};

struct dict {
    list_t * keys;
    list_t * values;
    int    * links;
    int    * hashtab;
    int      hashtab_size;
    int      refs;
};

struct frob {
    long cclass;
    data_t rep;
};

#include "ident.h"
#include "list.h"
#include "cdc_string.h"
#include "buffer.h"
#include "dict.h"
#include "object.h"
#include "data.h"

#endif

