/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/util.h
// ---
// Declarations for utility functions.
*/

#ifndef UTIL_H
#define UTIL_H

#define NUMBER_BUF_SIZE 32
typedef char Number_buf[NUMBER_BUF_SIZE];

#include <stdio.h>
#include <stdarg.h>
#include "cdc_types.h"

#define LCASE(c) lowercase[(int) c]
#define UCASE(c) uppercase[(int) c]

void init_util(void);
unsigned long hash(char *s);
unsigned long hash_case(char *s, int n);
long atoln(char *s, int n);
char *long_to_ascii(long num, Number_buf nbuf);
char *float_to_ascii(float num, Number_buf nbuf);
int strccmp(char *s1, char *s2);
int strnccmp(char *s1, char *s2, int n);
char *strcchr(char *s, int c);
char *strcstr(char *s, char *search);
long random_number(long n);
char *crypt_string(char *key, char *salt);
string_t *vformat(char *fmt, va_list arg);
string_t *format(char *fmt, ...);
char * timestamp(char * str);
void fformat(FILE *fp, char *fmt, ...);
string_t *fgetstring(FILE *fp);
char *english_type(int type);
char *english_integer(int n, Number_buf nbuf);
long parse_ident(char **sptr);
FILE *open_scratch_file(char *name, char *type);
void close_scratch_file(FILE *fp);
void init_scratch_file(void);
int parse_strcpy(char * s1, char * s2, int len);
int getarg(char * n, char ** buf, char * opt, char **argv, int * argc, void (*usage)(char *));
int is_valid_id(char * str, int len);

extern char lowercase[128];
extern char uppercase[128];

#endif

