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
Int  is_valid_ident(char * s);
Int  cur_lineno(void);

#endif

