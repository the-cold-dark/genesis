/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Convert text into tokens for yyparse().
*/

#include "defs.h"

#include <ctype.h>
#include "token.h"

#define NUM_RESERVED_WORDS (sizeof(reserved_words) / sizeof(*reserved_words))
#define SUBSCRIPT(c) ((c) & 0x7f)

INTERNAL char *string_token(char *s, Int len, Int *token_len);
INTERNAL char *identifier_token(char *s, Int len, Int *token_len);

static cList *code;
static Int cur_line, cur_pos;

/* Words with same first letters must be together. */
static struct {
    char *word;
    Int token;
} reserved_words[] = {
    { "any",			ANY },
    { "arg",			ARG },
    { "break",			BREAK },
    { "case",			CASE },
    { "catch",			CATCH },
    { "continue",		CONTINUE },
    { "default",		DEFAULT },
    { "disallow_overrides",	DISALLOW_OVERRIDES },
    { "else",			ELSE },
    { "filter",                 OP_FILTER },
    { "find",                   OP_FIND },
    { "for",			FOR },
    { "fork",			FORK },
    { "handler",		HANDLER },
    { "hash",                   OP_MAPHASH },
    { "if",			IF },
    { "in",			OP_IN },
    { "map",                    OP_MAP },
    { "pass",			PASS },
    { "return",			RETURN },
    { "switch",			SWITCH },
    { "to",			TO },
    { "var",			VAR },
    { "where",                  WHERE },
    { "while",			WHILE },
    { "with",			WITH },

    /* these are around for backwards/future compatability */

    /* cryptic reserved 'words' */
    { "(|",			CRITLEFT },
    { "(>",			PROPLEFT },
    { "<)",			PROPRIGHT },
    { "<=",			LE },
    { "..",			UPTO },
    { "|)",			CRITRIGHT },
    { "||",			OR },
    { "|",			OP_COND_OTHER_ELSE },
    { "#[",			START_DICT },
    { "`[",			START_BUFFER },

    { "&&",			AND },
    { "==",			EQ },
    { "=",			OP_ASSIGN },
    { "!=",			NE },
    { ">=",			GE },

    { "++",			INCREMENT },
    { "+=",			PLUS_EQ },
    { "--",			DECREMENT },
    { "-=",			MINUS_EQ },
    { "/=",			DIV_EQ },
    { "*=",			MULT_EQ },
    { "?=",			OPTIONAL_ASSIGN },
    { "?",			OP_COND_IF },
};

static struct {
    Int start;
    Int num;
} starting[128];

extern Pile *compiler_pile;		/* For allocating strings. */

void init_token(void)
{
    Int i, c;

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

void lex_start(cList * code_list) {
    code = code_list;
    cur_line = cur_pos = 0;
}

/* Returns if s can be parsed as an identifier. */
Bool is_valid_ident(char *s) {
    for (; *s; s++) {
	if (!isalnum(*s) && *s != '_')
	    return 0;
    }
    return 1;
}

Bool string_is_valid_ident(cStr * str) {
    char * s =   string_chars(str);
    int    len = string_length(str);

    for (; len; len--, s++) {
	if (!isalnum(*s) && *s != '_')
	    return 0;
    }
    return 1;
}

Bool is_reserved_word(char *s) {
    int start, i, j, len;
    char * word;

    len = strlen(s);

    start = starting[SUBSCRIPT(*s)].start;
    if (start != -1) {
	for (i = start; i < start + starting[SUBSCRIPT(*s)].num; i++) {
	    /* Compare remaining letters of word against s. */
	    word = reserved_words[i].word;
	    for (j = 1; j < len && word[j]; j++) {
		if (s[j] != word[j]) {
		    break;
                }
	    }

	    /* Comparison fails if we didn't match all the characters in word,
	     * or if word is an identifier and the next character in s isn't
	     * punctuation. */
	    if (word[j])
		continue;
	    if (isalpha(*s) && j < len && (isalnum(s[j]) || s[j] == '_'))
		continue;

	    return TRUE;
	}
    }
    return FALSE;
}

Int yylex(void)
{
    cData *d = (cData *)0;
    cStr *line, *float_buf;
    char *s = NULL, *word;
    Int len = 0, i, j, start, type;
    Bool negative;

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
	d = (cData *)0;
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
		if (s[j] != word[j]) {
		    break;
                }
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
        float_buf = string_new(32);

	/* Convert the string to a number. */
	yylval.num = 0;
	while (len && isdigit(*s)) {
            float_buf = string_addc(float_buf, *s);
	    yylval.num = yylval.num * 10 + (*s - '0');
	    s++, cur_pos++, len--;
	}

        if ((*s == '.' && isdigit(*(s+1))) || *s == 'e') {
	    Float f=yylval.num;

            f = atof(string_chars(float_buf));
            string_discard(float_buf);

	    if (*s=='.') {
	        Float muly=1;

	        s++, cur_pos++, len--;
		while (len && isdigit(*s)) {
		    muly/=10; f+=(*s - '0')*muly;
		    s++, cur_pos++, len--;
		}
	    }

	    if (len && *s=='e') {
		Int esign=0, evalue=0;

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
		if (esign) evalue =- evalue;
		if (evalue > 0)
		     while (evalue--) f*=10;
                else
		     while (evalue++) f/=10;
	    }
	    yylval.fnum=f;
	    return FLOAT;
	} else {	
            string_discard(float_buf);
	    return INTEGER;
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
	type = ((*s == '$') ? OBJNAME : ((*s == '\'') ? SYMBOL : T_ERROR));
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

    /* Check if it's a objnum. */
    if (*s == '#') {
        s++; len--; cur_pos++;
        if (len && *s == '-') {
            negative = YES;
            s++; len--; cur_pos++;
        } else {
            negative = NO;
        }
        if (len && isdigit(*s)) {
	    yylval.num = 0;
	    while (len && isdigit(*s)) {
	        yylval.num = yylval.num * 10 + (*s - '0');
	        s++, cur_pos++, len--;
	    }
            if (negative)
                yylval.num = -yylval.num;
        } else {
            yylval.num = INV_OBJNUM;
        }
        return OBJNUM;
    }

    if (len >= 2 && *s == '+' && s[1] == '+') {
        s += 2, cur_pos += 2, len -= 2;
        return INCREMENT;
    }

    if (len >= 2 && *s == '-' && s[1] == '-') {
        s += 2, cur_pos += 2, len -= 2;
        return DECREMENT;
    }

    /* None of the above. */
    cur_pos++;
    return *s;
}

Int cur_lineno(void)
{
    return cur_line + 1;
}

INTERNAL char * string_token(char * s, Int len, Int *token_len) {
    Int count = 0, i;
    char *p, *q;

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

    *token_len = (i == len) ? i : i + 1;
    return p;
}

/* Assumption: isalpha(*s) || *s == '_'. */
INTERNAL char *identifier_token(char *s, Int len, Int *token_len)
{
    Int count = 1, i;
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

