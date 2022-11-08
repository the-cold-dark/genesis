/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_util_h
#define cdc_util_h

#define NUMBER_BUF_SIZE (SIZEOF_LONG * 8) + 1
typedef char Number_buf[NUMBER_BUF_SIZE];

#include <stdarg.h>

/* needed for unlink() in Borland */
#ifdef __Win32__
#include <io.h>
#endif

#define NUM_CHARS 256
#define LCASE(c) lowercase[(int) c]
#define UCASE(c) uppercase[(int) c]

extern int lowercase[NUM_CHARS];
extern int uppercase[NUM_CHARS];

/* many system implementations of isprint() are EXTREMELY slow */
#define ISPRINT(_c_) ((Int) _c_ > 31 && (Int) _c_ < 127)

uLong hash_nullchar(const char *s);
uLong hash_string(cStr * str);
uLong hash_string_nocase(cStr * str);

void       init_util(void);
Long       atoln(const char *s, Int n);
char     * long_to_ascii(Long num, Number_buf nbuf);
char     * long_long_to_ascii(long long num, Number_buf nbuf);
char     * float_to_ascii(Float num, Number_buf nbuf);
Int        strccmp(const char *s1, const char *s2);
Int        strnccmp(const char *s1, const char *s2, Int n);
char     * strcchr(char *s, Int c);
char     * strcstr(char *s, const char *search);
Long       random_number(Long n);
cStr     * vformat(const char * fmt, va_list arg);
cStr     * format(const char * fmt, ...);
char     * timestamp(char * str);
void       fformat(FILE *fp, const char *fmt, ...);
cStr     * fgetstring(FILE *fp);
char     * english_type(Int type);
const char * english_integer(Int n, Number_buf nbuf);
Ident      parse_ident(char **sptr);
FILE     * open_scratch_file(const char *name, const char *type);
void       close_scratch_file(FILE *fp);
void       uninit_scratch_file(void);
void       init_scratch_file(void);
Int        parse_strcpy(char * s1, const char * s2, Int len);
bool       is_valid_id(const char * str, Int len);
Int        getarg(char * n,
                  char ** buf,
                  char * opt,
                  char **argv,
                  Int * argc,
                  void (*usage)(const char *));

#endif

