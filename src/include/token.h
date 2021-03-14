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
bool is_valid_ident(char * s);
bool string_is_valid_ident(cStr * str);
Int  cur_lineno(void);
bool is_reserved_word(char *s);


#endif

