/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_textdb_h
#define cdc_textdb_h

#define FORCE_NATIVES  1
#define IGNORE_NATIVES 2

#ifdef TEXTDB_C
Int use_natives;
#else
extern Int use_natives;
#endif

void compile_cdc_file(FILE * fp);
Int text_dump(Bool objnames);

#endif

