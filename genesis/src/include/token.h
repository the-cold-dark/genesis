/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Declarations for the lexer.
*/

#ifndef cdc_token_h
#define cdc_token_h

void init_token(void);
void lex_start(cList * code_lines);
Int  yylex(void);
Bool is_valid_ident(char * s);
Bool string_is_valid_ident(cStr * str);
Int  cur_lineno(void);
Bool is_reserved_word(char *s);


#endif

