/*
// Full copyright information is available in the file ../doc/CREDITS
//
// General utilities.
*/

#include "defs.h"

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include "util.h"
#include "token.h"
#include "macros.h"

#ifdef __MSVC__
#include <process.h>
#endif

#define FORMAT_BUF_INITIAL_LENGTH 48
#define MAX_SCRATCH 2

int lowercase[NUM_CHARS];
int uppercase[NUM_CHARS];

static Int  reserve_fds[MAX_SCRATCH];
static Int  fds_used;

INTERNAL void claim_fd(Int i);

void init_util(void) {
    int i;

    for (i = 0; i < NUM_CHARS; i++) {
	lowercase[i] = (int) (isupper(i) ? tolower(i) : i);
	uppercase[i] = (int) (islower(i) ? toupper(i) : i);
    }
    srand(time(NULL) + getpid());
}

uLong hash_nullchar(char * s) {
    uLong hashval = 0, g;

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

uLong hash_string(cStr * str) {
    uLong hashval = 0, g;
    int len;
    char * s;

    s = string_chars(str);
    len = string_length(str);

    /* Algorithm by Peter J. Weinberger. */
    for (; len; len--, s++) {
	hashval = (hashval << 4) + *s;
	g = hashval & 0xf0000000;
	if (g) {
	    hashval ^= g >> 24;
	    hashval ^= g;
	}
    }

    return hashval;
}

uLong hash_string_nocase(cStr * str) {
    uLong hashval = 0, g;
    int len;
    char * s;

    s = string_chars(str);
    len = string_length(str);

    /* Algorithm by Peter J. Weinberger. */
    for (; len; len--, s++) {
	hashval = (hashval << 4) + (*s & 0x5f);
	g = hashval & 0xf0000000;
	if (g) {
	    hashval ^= g >> 24;
	    hashval ^= g;
	}
    }

    return hashval;
}


Long atoln(char *s, Int n) {
    Long val = 0;

    while (n-- && isdigit(*s))
	val = val * 10 + *s++ - '0';
    return val;
}

char *long_to_ascii(Long num, Number_buf nbuf) {
#if DISABLED /* why?? */
    char *p = &nbuf[NUMBER_BUF_SIZE - 1];
    Int sign = 0;

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
    sprintf(nbuf, "%ld", (long) num);
    return nbuf;
#endif
}

char * float_to_ascii(Float num, Number_buf nbuf) {
    int i;
    sprintf (nbuf, "%g", num);
    for (i=0; nbuf[i]; i++)
      if (nbuf[i]=='.' || nbuf[i]=='e')
           return nbuf;
    nbuf[i]='.';
    nbuf[i+1]='0';
    nbuf[i+2]='\0';
    return nbuf;
}

/* Compare two strings, ignoring case. */
Int strccmp(char *s1, char *s2) {
    while (*s1 && LCASE(*s1) == LCASE(*s2))
	s1++, s2++;
    return LCASE(*s1) - LCASE(*s2);
}

/* Compare two strings up to n characters, ignoring case. */
Int strnccmp(char *s1, char *s2, Int n) {
    while (n-- && *s1 && LCASE(*s1) == LCASE(*s2))
	s1++, s2++;
    return (n >= 0) ? LCASE(*s1) - LCASE(*s2) : 0;
}

/* Look for c in s, ignoring case. */
char *strcchr(char *s, Int c) {
    c = LCASE(c);

    for (; *s; s++) {
	if (LCASE(*s) == c)
	    return s;
    }
    return (c) ? NULL : s;
}

char *strcstr(char *s, char *search) {
    char *p;
    Int search_len = strlen(search);

    if (!search_len)
        return NULL;

    for (p = strcchr(s, *search); p; p = strcchr(p + 1, *search)) {
	if (strnccmp(p, search, search_len) == 0)
	    return p;
    }

    return NULL;
}

/* A random number generator.  A lot of Unix rand() implementations don't
 * produce very random low bits, so we shift by eight bits if we can do that
 * without truncating the range. */
#ifndef RAND_MAX
#define RAND_MAX 256
#endif

Long random_number(Long n) {
    Long num = rand();
    if (!n)
      return 0;

#ifdef sys_sunos41
    if (n <= 256)
#else
    if (RAND_MAX >> 8 >= n)
#endif
	num >>= 8;
    return num % n;
}

/* Result must be copied before it can be re-used.  Non-reentrant. */
cStr * vformat(char * fmt, va_list arg) {
    cStr   * buf,
               * str;
    char       * p,
               * s;
    Number_buf   nbuf;

    buf = string_new(0);

    forever {

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
	    str = va_arg(arg, cStr *);
	    buf = string_add(buf, str);
	    break;

	  case 'd':
	    s = long_to_ascii(va_arg(arg, Int), nbuf);
	    buf = string_add_chars(buf, s, strlen(s));
	    break;

	  case 'l':
	    s = long_to_ascii(va_arg(arg, Long), nbuf);
	    buf = string_add_chars(buf, s, strlen(s));
	    break;

	  case 'D':
	    str = data_to_literal(va_arg(arg, cData *), DF_WITH_OBJNAMES);
	    if (string_length(str) > MAX_DATA_DISPLAY) {
		str = string_truncate(str, MAX_DATA_DISPLAY - 3);
		str = string_add_chars(str, "...", 3);
	    }
	    buf = string_add_chars(buf, string_chars(str), string_length(str));
	    string_discard(str);
	    break;

          case 'O': {
            cData d;
            d.type = OBJNUM;
            d.u.objnum = va_arg(arg, cObjnum);
            str = data_to_literal(&d, DF_WITH_OBJNAMES);
            buf = string_add_chars(buf, string_chars(str), string_length(str));
            string_discard(str);
          }
          break;

	  case 'I':
	    s = ident_name(va_arg(arg, Long));
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

cStr * format(char *fmt, ...) {
    va_list arg;
    cStr *str;

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
    static char tstr[LINE];
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
            tms->tm_year + 1900,
            tms->tm_hour,
            tms->tm_min);

    return s;
}

void fformat(FILE *fp, char *fmt, ...) {
    va_list arg;
    cStr *str;

    va_start(arg, fmt);

    str = vformat(fmt, arg);
    fputs(string_chars(str), fp);
    string_discard(str);

    va_end(arg);
}

#if DISABLED
cStr *fgetstring(FILE *fp) {
    cStr *line;
    Int len;
    char buf[BIGBUF];

    line = string_new(0);  

    while (fgets(buf, BIGBUF, fp)) {
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
#else
cStr *fgetstring(FILE *fp) {
    cStr        * line;
    Int           len;
    char        * p;

    /* we have a bigger line in general, but it reduces
       copies for the most common situation */
    line = string_new(BUF);
    p = string_chars(line);
    if (fgets(p, BUF, fp)) {
	len = strlen(p);
#ifdef __Win32__
        /* DOS and Windows text files may use \r\n or \n as a line termination */
        if ((len >= 2) && (p[len - 2] == '\r')) {
            p[len - 2] = (char) NULL;
            line->len = len - 2;
            return line;
        } else if (p[len - 1] == '\n') {
#else
        if (p[len - 1] == '\n') {
#endif
            p[len - 1] = (char) NULL;
            line->len = len - 1;
            return line;
        } else {
            char   buf[BIGBUF];

            line->len = len;

            /* drop to something less efficient for bigger cases */
            while (fgets(buf, BIGBUF, fp)) {
        	len = strlen(buf);
#ifdef __Win32__
	        /* DOS and Windows text files may use \r\n or \n as a line termination */
	        if ((len >= 2) && (buf[len - 2] == '\r')) {
	            line = string_add_chars(line, buf, len - 2);
		    return line;
                } else if (buf[len - 1] == '\n') {
#else
        	if (buf[len - 1] == '\n') {
#endif
        	    line = string_add_chars(line, buf, len-1);
        	    return line;
        	} else {
        	    line = string_add_chars(line, buf, len);
                }
            }
        }
    }

    if (line->len > 0) {
	return line;
    } else {
	string_discard(line);
	return NULL;
    }
}
#endif

char * english_type(Int type) {
    switch (type) {
      case INTEGER:	return "an integer";
      case FLOAT:	return "a float";
      case STRING:	return "a string";
      case OBJNUM:	return "a objnum";
      case LIST:	return "a list";
      case SYMBOL:	return "a symbol";
      case T_ERROR:	return "an error";
      case FROB:	return "a frob";
      case DICT:	return "a dictionary";
      case BUFFER:	return "a buffer";
    default:		{INSTANCE_RECORD(type, r); return r->name; }
    }
}

char *english_integer(Int n, Number_buf nbuf) {
    static char *first_eleven[] = {
	"no", "one", "two", "three", "four", "five", "six", "seven",
	"eight", "nine", "ten" };

    if (n <= 10)
	return first_eleven[n];
    else
	return long_to_ascii(n, nbuf);
}

Ident parse_ident(char **sptr) {
    cStr *str;
    char *s = *sptr;
    Long id;

    while (isalnum(*s) || *s == '_')
        s++;

    str = string_from_chars(*sptr, s - *sptr);

    id = ident_get(string_chars(str));
    string_discard(str);

    *sptr = s;

    return id;
}

FILE *open_scratch_file(char *name, char *type) {
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

void close_scratch_file(FILE *fp) {
    fclose(fp);
    claim_fd(--fds_used);
}

void init_scratch_file(void) {
    Int i;

    for (i = 0; i < MAX_SCRATCH; i++)
	claim_fd(i);
}

INTERNAL void claim_fd(Int i) {
#ifdef __Win32__
    reserve_fds[i] = open("null_file", O_WRONLY | O_CREAT, S_IREAD | S_IWRITE);
#else
    reserve_fds[i] = open("/dev/null", O_WRONLY);
#endif
    if (reserve_fds[i] == -1)
	panic("Couldn't reset reserved fd.");
}

void uninit_scratch_file(void) {
    Int i;

    for (i = 0; i < MAX_SCRATCH; i++)
        close(reserve_fds[i]);

#ifdef __Win32__
    unlink("null_file");
#endif
}

#define add_char(__s, __c) { *__s = __c; __s++; }

Int parse_strcpy(char * b1, char * b2, Int slen) {
    register Int l = slen, len = slen;
    register char * s = b2, * b = b1;

    while (l > 0) {
        if (*s == '\\') {
            s++, l--;
            switch (*s) {
                case 't':
                    add_char(b, '\t');
                    len--;
                    break;
                case 'n':
                    add_char(b, '\r');
                    add_char(b, '\n');
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

Int getarg(char * n,
           char **buf,
           char * opt,
           char **argv,
           Int  * argc,
           void (*usage)(char *))
{
    char * p = opt; 
    
    p++;
    if (*p != (char) NULL) {
        *buf = p;

        return 0;
    }

    if ((*argc - 1) <= 0) {
        usage(n);
        fprintf(stderr, "** No followup argument to -%s.\n", opt);
        exit(1);
    }

    argv++;
    *buf = *argv;
    *argc -= 1;

    /* we don't want spaces */
    p = *buf;
    while (isspace(*p)) p++;

    /* not likely, but possible */
    if (strlen(p) == 0) {
        usage(n);
        fprintf(stderr, "** Invalid followup argument to -%s.\n", opt);
        exit(1);
    }

    *buf = p;

    return 1;
}

Int is_valid_id(char * str, Int len) {
    while (len--) {
        if (!isalnum(*str) && *str != '_')
            return 0;
        str++;
     }
     return 1;
}

void set_argv0(char * str) {
    
}
