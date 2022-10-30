/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_string_h
#define cdc_string_h

#include "regexp.h"

cStr * string_new(Int len);
cStr * string_empty(Int size);
cStr * string_from_chars(const char * s, Int len);
cStr * string_of_char(Int c, Int len);
cStr * string_dup(cStr * str);

cBuf * string_pack(cBuf *buf, cStr *str);
cStr * string_unpack(cBuf *buf, Long *buf_pos);
Int    string_packed_size(cStr *str, int memory_size);

Int    string_cmp(cStr * str1, cStr * str2);
cStr * string_add(cStr * str1, cStr * str2);
cStr * string_add_chars(cStr * str, const char * s, Int len);
cStr * string_addc(cStr * str, Int c);
cStr * string_add_padding(cStr * str,
                          const char     * filler,
                          Int        len,
                          Int        padding);
cStr * string_truncate(cStr * str, Int len);
cStr * string_substring(cStr * str, Int start, Int len);
cStr * string_uppercase(cStr * str);
cStr * string_lowercase(cStr * str);
regexp * string_regexp(cStr * str);
void   string_discard(cStr * str);
cStr * string_parse(char * *sptr);
cStr * string_add_unparsed(cStr * str, const char * s, Int len);
char * regerror(const char * msg);
int    string_index(cStr * str, cStr * sub, int origin);
cStr * string_prep(cStr *str, Int start, Int len);

#define string_length(__s) ((Int) __s->len)
#define string_chars(__s) ((char *) __s->s + __s->start)

#endif

