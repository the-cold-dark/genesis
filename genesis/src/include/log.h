/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_log_h
#define cdc_log_h

void panic(char *s, ...);
void fail_to_start(char *s);
void write_log(char *s, ...);
void write_err(char *s, ...);

#endif

