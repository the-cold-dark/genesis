/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Procedures to handle logging and fatal errors.
*/

#include "defs.h"

#include <sys/types.h>
#include <stdarg.h>
#include "cache.h"
#include "util.h"
#include "sig.h"
#include "execute.h"

void panic(const char * s, ...) {
    va_list vargs;
    static bool panic_state = false;

    va_start(vargs,s);
    fprintf(errfile, "[%s] %s: ", timestamp(NULL),
            (panic_state ? "RECURSIVE PANIC" : "PANIC"));
    vfprintf(errfile,s,vargs);
    va_end(vargs);
    fputc('\n',errfile);

    if (!panic_state) {
        panic_state = true;
        fprintf(errfile, "[%s] doing binary dump...", timestamp(NULL));
        cache_sync();
        fputs("Done\n", errfile);
        log_all_task_stacks(false, write_err);
    }

    fprintf(errfile, "[%s] Creating Core Image...\n", timestamp(NULL));
    dump_core_and_exit();
}

void fail_to_start(const char *s) {
    fprintf(errfile, "[%s] FAILED TO START: %s\n", timestamp(NULL), s);

    exit(1);
}

void write_log(const char *fmt, ...) {
    va_list arg;
    cStr *str;

    va_start(arg, fmt);

    str = vformat(fmt, arg);

    fputs(string_chars(str), logfile);
    fputc('\n', logfile);

    fflush(logfile);

    string_discard(str);
    va_end(arg);
}

void write_err(const char *fmt, ...) {
    va_list arg;
    cStr *str;

    va_start(arg, fmt);
    str = vformat(fmt, arg);
    va_end(arg);

    fprintf(errfile, "[%s] %s\n", timestamp(NULL), string_chars(str));
    fflush(errfile);

    string_discard(str);
}

void log_current_task_stack(bool want_lineno, void (logroutine)(const char*,...))
{
    cList * stack;

    stack = vm_stack(cur_frame, want_lineno);
    log_task_stack(task_id, stack, logroutine);
    list_discard(stack);
}

void log_all_task_stacks(bool want_lineno, void (logroutine)(const char*,...))
{
    VMState * vm;
    cList   * stack;

    (logroutine)("Current task:");
    stack = vm_stack(cur_frame, want_lineno);
    log_task_stack(task_id, stack, logroutine);
    list_discard(stack);

    (logroutine)("Suspended tasks:");
    for (vm = suspended;  vm;  vm = vm->next) {
        stack = vm_stack(vm->cur_frame, want_lineno);
        log_task_stack(vm->task_id, stack, logroutine);
        list_discard(stack);
    }

    (logroutine)("Paused tasks:");
    for (vm = preempted;  vm;  vm = vm->next) {
        stack = vm_stack(vm->cur_frame, want_lineno);
        log_task_stack(vm->task_id, stack, logroutine);
        list_discard(stack);
    }
}

