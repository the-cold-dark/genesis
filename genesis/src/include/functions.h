/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/functions.h
// ---
// Function declarations.
*/

#ifndef _functions_h_
#define _functions_h_

void func_null_function(void);

void func_add_parameter(void);
void func_del_parameter(void);
void func_parameters(void);
void func_set_var(void);
void func_get_var(void);
void func_clear_var(void);
void func_compile_to_method(void);
void func_set_method_state(void);
void func_set_method_flags(void);
void func_method_state(void);
void func_method_flags(void);
void func_method_args(void);
void func_methods(void);
void func_find_method(void);
void func_find_next_method(void);
void func_list_method(void);
void func_del_method(void);
void func_parents(void);
void func_children(void);
void func_ancestors(void);
void func_has_ancestor(void);
void func_size(void);
void func_create(void);
void func_chparents(void);
void func_destroy(void);
void func_log(void);
void func_backup(void);
void func_binary_dump(void);
void func_text_dump(void);
void func_shutdown(void);
void func_execute(void);
void func_set_heartbeat(void);
void func_data(void);
void func_add_objname(void);
void func_del_objname(void);
void func_cancel(void);
void func_suspend(void);
void func_resume(void);
void func_pause(void);
void func_tasks(void);
void func_tick(void);
void func_callers(void);
void func_bind_function(void);
void func_unbind_function(void);
void func_method(void);
void func_this(void);
void func_definer(void);
void func_sender(void);
void func_caller(void);
void func_task_id(void);
void func_ticks_left(void);
void func_type(void);
void func_class(void);
void func_toint(void);
void func_tofloat(void);
void func_tostr(void);
void func_toliteral(void);
void func_todbref(void);
void func_tosym(void);
void func_toerr(void);
void func_valid(void);
void func_error(void);
void func_traceback(void);
void func_throw(void);
void func_rethrow(void);

#endif
