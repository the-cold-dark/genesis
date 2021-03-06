/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_decode_h
#define cdc_decode_h

Int      line_number(Method * method, Int pc);

cList * decompile(Method * method,
                   Obj * object,
                   Int        increment,
                   Int        parens);

#define FMT_DEFAULT     0
#define FMT_FULL_PARENS 1
#define FMT_FULL_BRACES 2

#endif

