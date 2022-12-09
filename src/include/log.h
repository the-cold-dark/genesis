/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_log_h
#define cdc_log_h

#include <stdnoreturn.h>

noreturn void panic(const char *s, ...);
noreturn void fail_to_start(const char *s);
void write_log(const char *s, ...);
void write_err(const char *s, ...);
void log_current_task_stack(bool want_lineno, void (logroutine)(const char*,...));
void log_all_task_stacks(bool want_lineno, void (logroutine)(const char*,...));

#endif

