/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/cdc_string.h
// ---
// Declarations for string handling.
*/

#ifndef _string_h_
#define _string_h_

#include <stdio.h>
#include "cdc_types.h"
#include "regexp.h"

string_t * string_new(int len);
string_t * string_empty(int size);
string_t * string_from_chars(char * s, int len);
string_t * string_of_char(int c, int len);
string_t * string_dup(string_t * str);
#if 0
int        string_length(string_t * str);
char     * string_chars(string_t * str);
#endif
void       string_pack(string_t * str, FILE * fp);
string_t * string_unpack(FILE * fp);
int        string_packed_size(string_t * str);
int        string_cmp(string_t * str1, string_t * str2);
string_t * string_add(string_t * str1, string_t * str2);
string_t * string_add_chars(string_t * str, char * s, int len);
string_t * string_addc(string_t * str, int c);
string_t * string_add_padding(string_t * str,
                              char     * filler,
                              int        len,
                              int        padding);
string_t * string_truncate(string_t * str, int len);
string_t * string_substring(string_t * str, int start, int len);
string_t * string_uppercase(string_t * str);
string_t * string_lowercase(string_t * str);
regexp   * string_regexp(string_t * str);
void       string_discard(string_t * str);
string_t * string_parse(char * *sptr);
string_t * string_add_unparsed(string_t * str, char * s, int len);
char     * regerror(char * msg);

#define string_length(__s) ((int) __s->len)
#define string_chars(__s) ((char *) __s->s + __s->start)

#endif

