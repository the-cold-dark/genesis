/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: util.c
// ---
// General utilities.
*/

#include "config.h"
#include "defs.h"

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include "util.h"
#include "cdc_string.h"
#include "data.h"
#include "ident.h"
#include "token.h"
#include "memory.h"
#include "log.h"

#define FORMAT_BUF_INITIAL_LENGTH 48
#define MAX_SCRATCH 2

/* crypt() is not POSIX. */
extern char * crypt(const char *, const char *);

       char lowercase[128];
       char uppercase[128];
static int  reserve_fds[MAX_SCRATCH];
static int  fds_used;

INTERNAL void claim_fd(int i);

void init_util(void)
{
    int i;

    for (i = 0; i < 128; i++) {
	lowercase[i] = (isupper(i) ? tolower(i) : i);
	uppercase[i] = (islower(i) ? toupper(i) : i);
    }
    srand(time(NULL) + getpid());
}

unsigned long hash(char *s)
{
    unsigned long hashval = 0, g;

    /* Algorithm by Peter J. Weinberger. */
    for (; *s; s++) {
	hashval = (hashval << 4) + *s;
	g = hashval & 0xf0000000;
	if (g) {
	    hashval ^= g >> 24;
	    hashval ^= g;
	}
    }
    return hashval;
}

unsigned long hash_case(char *s, int n)
{
    unsigned long hashval = 0, g;
    int i;

    /* Algorithm by Peter J. Weinberger. */
    for (i = 0; i < n; i++) {
	hashval = (hashval << 4) + (s[i] & 0x5f);
	g = hashval & 0xf0000000;
	if (g) {
	    hashval ^= g >> 24;
	    hashval ^= g;
	}
    }
    return hashval;
}

long atoln(char *s, int n)
{
    long val = 0;

    while (n-- && isdigit(*s))
	val = val * 10 + *s++ - '0';
    return val;
}

char *long_to_ascii(long num, Number_buf nbuf)
{
#if DISABLED /* why?? */
    char *p = &nbuf[NUMBER_BUF_SIZE - 1];
    int sign = 0;

    *p-- = 0;
    if (num < 0) {
	sign = 1;
	num = -num;
    } else if (!num) {
	*p-- = '0';
    }
    while (num) {
	*p-- = num % 10 + '0';
	num /= 10;
    }
    if (sign)
	*p-- = '-';
    return p + 1;
#else
    *nbuf++ = (char) 0;
    sprintf(nbuf, "%ld", num);
    return nbuf;
#endif
}

char * float_to_ascii(float num, Number_buf nbuf) {
    sprintf (nbuf,"%f",num);
    return nbuf;
}

/* Compare two strings, ignoring case. */
int strccmp(char *s1, char *s2)
{
    while (*s1 && LCASE(*s1) == LCASE(*s2))
	s1++, s2++;
    return LCASE(*s1) - LCASE(*s2);
}

/* Compare two strings up to n characters, ignoring case. */
int strnccmp(char *s1, char *s2, int n)
{
    while (n-- && *s1 && LCASE(*s1) == LCASE(*s2))
	s1++, s2++;
    return (n >= 0) ? LCASE(*s1) - LCASE(*s2) : 0;
}

/* Look for c in s, ignoring case. */
char *strcchr(char *s, int c)
{
    c = LCASE(c);

    for (; *s; s++) {
	if (LCASE(*s) == c)
	    return s;
    }
    return (c) ? NULL : s;
}

char *strcstr(char *s, char *search)
{
    char *p;
    int search_len = strlen(search);

    for (p = strcchr(s, *search); p; p = strcchr(p + 1, *search)) {
	if (strnccmp(p, search, search_len) == 0)
	    return p;
    }

    return NULL;
}

/* A random number generator.  A lot of Unix rand() implementations don't
 * produce very random low bits, so we shift by eight bits if we can do that
 * without truncating the range. */
long random_number(long n)
{
    long num = rand();
    if (!n)
      return 0;

#ifdef SUNOS
    if (256           >= n)
#else
    if (RAND_MAX >> 8 >= n)
#endif
	num >>= 8;
    return num % n;
}

/* Encrypt a string.  The salt can be NULL. */
char *crypt_string(char *key, char *salt)
{
    char rsalt[2];

    if (!salt) {
	rsalt[0] = random_number(95) + 32;
	rsalt[1] = random_number(95) + 32;
	salt = rsalt;
    }

    return crypt(key, salt);
}

/* Result must be copied before it can be re-used.  Non-reentrant. */
string_t *vformat(char *fmt, va_list arg) {
    string_t   * buf,
               * str;
    char       * p,
               * s;
    Number_buf   nbuf;

    buf = string_new(0);

    while (1) {

	/* Find % or end of string. */
	p = strchr(fmt, '%');
	if (!p || !p[1]) {
	    /* No more percents; copy rest and stop. */
	    buf = string_add_chars(buf, fmt, strlen(fmt));
	    break;
	}
	buf = string_add_chars(buf, fmt, p - fmt);

	switch (p[1]) {

	  case '%':
	    buf = string_addc(buf, '%');
	    break;

	  case 's':
	    s = va_arg(arg, char *);
	    buf = string_add_chars(buf, s, strlen(s));
	    break;

	  case 'S':
	    str = va_arg(arg, string_t *);
	    buf = string_add(buf, str);
	    break;

	  case 'd':
	    s = long_to_ascii(va_arg(arg, int), nbuf);
	    buf = string_add_chars(buf, s, strlen(s));
	    break;

	  case 'l':
	    s = long_to_ascii(va_arg(arg, long), nbuf);
	    buf = string_add_chars(buf, s, strlen(s));
	    break;

	  case 'D':
	    str = data_to_literal(va_arg(arg, data_t *));
	    if (string_length(str) > MAX_DATA_DISPLAY) {
		str = string_truncate(str, MAX_DATA_DISPLAY - 3);
		str = string_add_chars(str, "...", 3);
	    }
	    buf = string_add_chars(buf, string_chars(str), string_length(str));
	    string_discard(str);
	    break;

          case 'O': {
            data_t d;
            d.type = OBJNUM;
            d.u.objnum = va_arg(arg, objnum_t);
            str = data_to_literal(&d);
            buf = string_add_chars(buf, string_chars(str), string_length(str));
            string_discard(str);
          }
          break;

	  case 'I':
	    s = ident_name(va_arg(arg, long));
	    if (is_valid_ident(s))
		buf = string_add_chars(buf, s, strlen(s));
	    else
		buf = string_add_unparsed(buf, s, strlen(s));
	    break;
	}

	fmt = p + 2;
    }

    va_end(arg);
    return buf;
}

string_t *format(char *fmt, ...)
{
    va_list arg;
    string_t *str;

    va_start(arg, fmt);
    str = vformat(fmt, arg);
    va_end(arg);
    return str;
}

/* 
// builds a timestamp in the almost rfc850 format (DD MMM YY HH:MM)
//
// Added: 30 Jul 1995 - BJG
*/
char * timestamp (char * str) {
    time_t      t;
    struct tm * tms;
    static char tstr[WORD];
    char      * s;
    char      * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                            "Aug", "Sep", "Oct", "Nov", "Dec"};

    if (str != NULL)
        s = str;
    else
        s = tstr;

    time(&t);
    tms = localtime(&t);
    sprintf(s, "%d %3s %2d %d:%.2d",
            tms->tm_mday,
            months[tms->tm_mon],
            tms->tm_year,
            tms->tm_hour,
            tms->tm_min);

    return s;
}

void fformat(FILE *fp, char *fmt, ...)
{
    va_list arg;
    string_t *str;

    va_start(arg, fmt);

    str = vformat(fmt, arg);
    fputs(string_chars(str), fp);
    string_discard(str);

    va_end(arg);
}

string_t *fgetstring(FILE *fp)
{
    string_t *line;
    int len;
    char buf[1000];

    line = string_new(0);
    while (fgets(buf, 1000, fp)) {
	len = strlen(buf);
	if (buf[len - 1] == '\n') {
	    line = string_add_chars(line, buf, len-1);
	    return line;
	} else
	    line = string_add_chars(line, buf, len);
    }

    if (line->len) {
	return line;
    } else {
	string_discard(line);
	return NULL;
    }
}

char *english_type(int type)
{
    switch (type) {
      case INTEGER:	return "an integer";
      case FLOAT:	return "a float";
      case STRING:	return "a string";
      case OBJNUM:	return "a objnum";
      case LIST:	return "a list";
      case SYMBOL:	return "a symbol";
      case ERROR:	return "an error";
      case FROB:	return "a frob";
      case DICT:	return "a dictionary";
      case BUFFER:	return "a buffer";
      default:		return "a mistake";
    }
}

char *english_integer(int n, Number_buf nbuf)
{
    static char *first_eleven[] = {
	"no", "one", "two", "three", "four", "five", "six", "seven",
	"eight", "nine", "ten" };

    if (n <= 10)
	return first_eleven[n];
    else
	return long_to_ascii(n, nbuf);
}

long parse_ident(char **sptr)
{
    string_t *str;
    char *s = *sptr;
    long id;

    if (*s == '"') {
	str = string_parse(&s);
    } else {
	while (isalnum(*s) || *s == '_')
	    s++;
	str = string_from_chars(*sptr, s - *sptr);
    }

    id = ident_get(string_chars(str));
    string_discard(str);
    *sptr = s;
    return id;
}

FILE *open_scratch_file(char *name, char *type)
{
    FILE *fp;

    if (fds_used == MAX_SCRATCH)
	return NULL;

    close(reserve_fds[fds_used++]);
    fp = fopen(name, type);
    if (!fp) {
	claim_fd(--fds_used);
	return NULL;
    }
    return fp;
}

void close_scratch_file(FILE *fp)
{
    fclose(fp);
    claim_fd(--fds_used);
}

void init_scratch_file(void)
{
    int i;

    for (i = 0; i < MAX_SCRATCH; i++)
	claim_fd(i);
}

INTERNAL void claim_fd(int i)
{
    reserve_fds[i] = open("/dev/null", O_WRONLY);
    if (reserve_fds[i] == -1)
	panic("Couldn't reset reserved fd.");
}

#define add_char(__s, __c) { *__s = __c; __s++; }

int parse_strcpy(char * b1, char * b2, int slen) {
    int l = slen, len = slen;
    char * s = b2, * b = b1;

    while (l > 0) {
        if (*s == '\\') {
            s++, l--;
            switch (*s) {
                case 'n':
                    add_char(b, '\n');
                    len--;
                    break;
                case 'r':
                    add_char(b, '\r');
                    len--;
                    break;
                case '\\':
                    add_char(b, '\\');
                    len--;
                    break;
                default:
                    add_char(b, '\\');
                    add_char(b, *s);
                    break;
            }
            s++, l--;
        } else {
            add_char(b, *s);
            s++, l--;
        }
    }

    return len;
}

#undef add_char

int getarg(char * n,
           char **buf,
           char * opt,
           char **argv,
           int  * argc,
           void (*usage)(char *))
{
    char * p = opt; 
    
    p++;
    if (*p != (char) NULL) {
        *buf = p;

        return 0;
    }

    if ((*argc - 1) < 0) {
        usage(n);
        fprintf(stderr, "** No followup argument to -%s.\n", opt);
        exit(1);
    }

    argv++;
    *buf = *argv;
    *argc -= 1;

    return 1;
}

int is_valid_id(char * str, int len) {
    while (len--) {
        if (!isalnum(*str) && *str != '_')
            return 0;
        str++;
     }
     return 1;
}
