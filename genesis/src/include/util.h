/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_util_h
#define cdc_util_h

#define NUMBER_BUF_SIZE (SIZEOF_LONG * 8) + 1
typedef char Number_buf[NUMBER_BUF_SIZE];

#include <stdarg.h>

#define LCASE(c) lowercase[(Int) c]
#define UCASE(c) uppercase[(Int) c]

/* many system implementations of isprint() are EXTREMELY slow */
#define ISPRINT(_c_) ((Int) _c_ > 31 && (Int) _c_ < 127)

extern char lowercase[128];
extern char uppercase[128];

uLong hash(char *s);
uLong hash_case(char *s, Int n);

void       init_util(void);
Long       atoln(char *s, Int n);
char     * long_to_ascii(Long num, Number_buf nbuf);
char     * float_to_ascii(float num, Number_buf nbuf);
Int        strccmp(char *s1, char *s2);
Int        strnccmp(char *s1, char *s2, Int n);
char     * strcchr(char *s, Int c);
char     * strcstr(char *s, char *search);
Long       random_number(Long n);
cStr     * vformat(char * fmt, va_list arg);
cStr     * format(char * fmt, ...);
char     * timestamp(char * str);
void       fformat(FILE *fp, char *fmt, ...);
cStr     * fgetstring(FILE *fp);
char     * english_type(Int type);
char     * english_integer(Int n, Number_buf nbuf);
Long       parse_ident(char **sptr);
FILE     * open_scratch_file(char *name, char *type);
void       close_scratch_file(FILE *fp);
void       init_scratch_file(void);
Int        parse_strcpy(char * s1, char * s2, Int len);
Int        is_valid_id(char * str, Int len);
Int        getarg(char * n,
                  char ** buf,
                  char * opt,
                  char **argv,
                  Int * argc,
                  void (*usage)(char *));

#endif

