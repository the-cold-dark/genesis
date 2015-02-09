/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_log_h
#define cdc_log_h

void panic(char *s, ...);
void fail_to_start(char *s);
void write_log(char *s, ...);
void write_err(char *s, ...);
void log_current_task_stack(Bool want_lineno, void (logroutine)(char*,...));
void log_all_task_stacks(Bool want_lineno, void (logroutine)(char*,...));

#endif

