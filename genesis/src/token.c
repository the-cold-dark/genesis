/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: token.c
// ---
// Convert text into tokens for yyparse().
*/

#include <ctype.h>
#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "token.h"
#include "memory.h"
#include "data.h"

#define NUM_RESERVED_WORDS (sizeof(reserved_words) / sizeof(*reserved_words))
#define SUBSCRIPT(c) ((c) & 0x7f)

internal char *string_token(char *s, int len, int *token_len);
internal char *identifier_token(char *s, int len, int *token_len);

static list_t *code;
static cur_line, cur_pos;

/* Words with same first letters must be together. */
static struct {
    char *word;
    int token;
} reserved_words[] = {
    { "any",			ANY },
    { "arg",			ARG },
    { "atomic",			ATOMIC },
    { "break",			BREAK },
    { "case",			CASE },
    { "catch",			CATCH },
    { "continue",		CONTINUE },
    { "default",		DEFAULT },
    { "disallow_overrides",	DISALLOW_OVERRIDES },
    { "else",			ELSE },
    { "for",			FOR },
    { "fork",			FORK },
    { "handler",		HANDLER },
    { "if",			IF },
    { "in",			IN },
    { "non_atomic",		NON_ATOMIC },
    { "pass",			PASS },
    { "return",			RETURN },
    { "switch",			SWITCH },
    { "to",			TO },
    { "var",			VAR },
    { "while",			WHILE },
    { "with",			WITH },
    { "(|",			CRITLEFT },
    { "(>",			PROPLEFT },
    { "<)",			PROPRIGHT },
    { "<=",			LE },
    { "..",			UPTO },
    { "|)",			CRITRIGHT },
    { "||",			OR },
    { "#[",			START_DICT },
    { "`[",			START_BUFFER },

    { "&&",			AND },
    { "==",			EQ },
    { "!=",			NE },
    { ">=",			GE }
#if 0
    ,
    { ">>",			SR },
    { "<<",			SL }
#endif
};

static struct {
    int start;
    int num;
} starting[128];

extern Pile *compiler_pile;		/* For allocating strings. */

void init_token(void)
{
    int i, c;

    for (i = 0; i < 128; i++)
	starting[i].start = -1;

    i = 0;
    while (i < NUM_RESERVED_WORDS) {
	c = SUBSCRIPT(*reserved_words[i].word);
	starting[c].start = i;
	starting[c].num = 1;
	for (i++; i < NUM_RESERVED_WORDS && *reserved_words[i].word == c; i++)
	    starting[c].num++;
    }
}

void lex_start(list_t * code_list) {
    code = code_list;
    cur_line = cur_pos = 0;
}

/* Returns if s can be parsed as an identifier. */
int is_valid_ident(char *s)
{
    while (*s) {
	if (!isalnum(*s) && *s != '_')
	    return 0;
	s++;
    }
    return 1;
}

int yylex(void)
{
    data_t *d = (data_t *)0;
    string_t *line;
    char *s = NULL, *word;
    int len = 0, i, j, start, type;

    /* Find the beginning of the next token. */
    while (cur_line < list_length(code)) {
	/* Fetch text and length of current line. */
	d = list_elem(code, cur_line);
	line = d->u.str;
	s = string_chars(line);
	len = string_length(line);

	/* Scan over line for a non-space character. */
	while (cur_pos < len && isspace(s[cur_pos]))
	    cur_pos++;

	/* If we didn't hit the end, return the character we stopped at. */
	if (cur_pos < len)
	    break;

	/* Go on to the next line. */
	cur_line++;
	cur_pos = 0;
	d = (data_t *)0;
    }
    if (!d) {
	return 0;
    } else {
	s += cur_pos;
	len -= cur_pos;
    }

    /* Check if it's a reserved word. */
    start = starting[SUBSCRIPT(*s)].start;
    if (start != -1) {
	for (i = start; i < start + starting[SUBSCRIPT(*s)].num; i++) {
	    /* Compare remaining letters of word against s. */
	    word = reserved_words[i].word;
	    for (j = 1; j < len && word[j]; j++) {
		if (s[j] != word[j])
		    break;
	    }

	    /* Comparison fails if we didn't match all the characters in word,
	     * or if word is an identifier and the next character in s isn't
	     * punctuation. */
	    if (word[j])
		continue;
	    if (isalpha(*s) && j < len && (isalnum(s[j]) || s[j] == '_'))
		continue;

	    cur_pos += j;
	    return reserved_words[i].token;
	}
    }

    /* Check if it's an identifier. */
    if (isalpha(*s) || *s == '_') {
	yylval.s = identifier_token(s, len, &i);
	cur_pos += i;
	return IDENT;
    }

    /* Check if it's a number. */
    if (isdigit(*s)) {
	/* Convert the string to a number. */
	yylval.num = 0;
	while (len && isdigit(*s)) {
	    yylval.num = yylval.num * 10 + (*s - '0');
	    s++, cur_pos++, len--;
	}

        if (len && *s!='.' && *s!='e')
	    return INTEGER;

	{
	    float f=yylval.num;

	    if (*s=='.') {
	        float muly=1;

	        s++, cur_pos++, len--;
		while (len && isdigit(*s)) {
		    muly/=10; f+=(*s - '0')*muly;
		    s++, cur_pos++, len--;
		}
	    }

	    if (len && *s=='e') {
		int esign=0, evalue=0;

	        s++, cur_pos++, len--;
		if (len && *s=='-') {
		    esign=1;
		    s++, cur_pos++, len--;
		}
		else if (len && *s=='+') {
		    esign=0;
		    s++, cur_pos++, len--;
		}
		while (len && isdigit(*s)) {
		    evalue=evalue * 10 + (*s - '0');
		    s++, cur_pos++, len--;
		}
		if (esign) evalue=-evalue;
		if (evalue>0)
		     while (evalue--) f*=10;
                else
		     while (evalue++) f*=10;
	    }
	    yylval.fnum=f;
	    return FLOAT;
	}	
    }

    /* Check if it's a string. */
    if (*s == '"') {
	yylval.s = string_token(s, len, &i);
	cur_pos += i;
	return STRING;
    }

    /* Check if it's an object literal, symbol, or error code. */
    if ((*s == '$' || *s == '\'' || *s == '~')) {
	type = ((*s == '$') ? NAME : ((*s == '\'') ? SYMBOL : ERROR));
	if (len > 1 && s[1] == '"') {
	    yylval.s = string_token(s + 1, len - 1, &i);
	    cur_pos += i + 1;
	    return type;
	} else if (isalnum(s[1]) || s[1] == '_') {
	    yylval.s = identifier_token(s + 1, len - 1, &i);
	    cur_pos += i + 1;
	    return type;
	}
    }

    /* Check if it's a comment. */
    if (len >= 2 && *s == '/' && s[1] == '/') {
	/* Copy in text after //, and move to next line. */
	yylval.s = PMALLOC(compiler_pile, char, len - 1);
	MEMCPY(yylval.s, s + 2, len - 2);
	yylval.s[len - 2] = 0;
	cur_line++;
	cur_pos = 0;
	return COMMENT;
    }

    /* Check if it's a dbref. */
    if (len >= 2 && *s == '#' && isdigit(s[1])) {
	/* Convert the string to a number. */
	s++, cur_pos++, len--;
	yylval.num = 0;
	while (len && isdigit(*s)) {
	    yylval.num = yylval.num * 10 + (*s - '0');
	    s++, cur_pos++, len--;
	}
	return DBREF;
    }

    /* None of the above. */
    cur_pos++;
    return *s;
}

int cur_lineno(void)
{
    return cur_line + 1;
}

internal char *string_token(char * s, int len, int *token_len)
{
    int count = 0, i;
    char *p, *q;

#if 0
    /* Count characters in string. */
    for (i = 1; i < len && s[i] != '"'; i++) {
	if (s[i] == '\\' && i < len - 1)
	    i++;
	count++;
    }

    /* Allocate space and copy. */
    q = p = PMALLOC(compiler_pile, char, count + 1);
    for (i = 1; i < len && s[i] != '"'; i++) {
	if (s[i] == '\\' && i < len - 1)
	    i++;
	*q++ = s[i];
    }
    *q = 0;
#else
    /* Count the length */
    for (i = 1; i < len && s[i] != '"'; i++) {
        if (s[i] == '\\' && i < len -1 && (s[i+1] == '"' || s[i+1] == '\\'))
	    i++;
	count++;
    }

    /* Allocate space and copy. */
    q = p = PMALLOC(compiler_pile, char, count + 1);
    for (i = 1; i < len && s[i] != '"'; i++) {
	if (s[i] == '\\' && i < len - 1 && (s[i+1] == '"' || s[i+1] == '\\'))
	    i++;
	*q++ = s[i];
    }
    *q = 0;
#endif

    *token_len = (i == len) ? i : i + 1;
    return p;
}

/* Assumption: isalpha(*s) || *s == '_'. */
internal char *identifier_token(char *s, int len, int *token_len)
{
    int count = 1, i;
    char *p;

    /* Count characters in identifier. */
    for (i = 1; i < len && (isalnum(s[i]) || s[i] == '_'); i++)
	 count++;

    /* Allocate space and copy. */
    p = PMALLOC(compiler_pile, char, count + 1);
    MEMCPY(p, s, count);
    p[count] = 0;

    *token_len = count;
    return p;
}

