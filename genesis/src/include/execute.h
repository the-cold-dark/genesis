/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/execute.h
// ---
// Declarations for executing ColdC tasks.
*/

#ifndef _execute_h_
#define _execute_h_

typedef struct frame Frame;
typedef struct error_action_specifier Error_action_specifier;
typedef struct handler_info Handler_info;
typedef struct vmstate VMState;
typedef struct vmstack VMStack;

#include <sys/types.h>
#include <stdarg.h>
#include "data.h"
#include "object.h"
#include "io.h"

/* We use the MALLOC_DELTA defines to keep table sizes thirty-two bytes less
 * than a power of two, which is helpful on buddy systems. */
#define STACK_MALLOC_DELTA 4
#define ARG_STACK_MALLOC_DELTA 8

struct vmstack {
  data_t *stack;
  int stack_size;
  int *arg_starts, arg_size;
  VMStack *next;
};

struct vmstate {
    Frame *cur_frame;
    connection_t *cur_conn;
    data_t *stack;
    int stack_pos, stack_size;
    int *arg_starts, arg_pos, arg_size;
    int task_id;
    int paused;
    VMState *next;
};

struct frame {
    object_t *object;
    Dbref sender;
    Dbref caller;
    method_t *method;
    long *opcodes;
    int pc;
    int last_opcode;
    int ticks;
    int stack_start;
    int var_start;
    Error_action_specifier *specifiers;
    Handler_info *handler_info;
    Frame *caller_frame;
};

struct error_action_specifier {
    int type;
    int stack_pos;
    union {
	struct {
	    int end;
	} critical;
	struct {
	    int end;
	} propagate;
	struct {
	    int error_list;
	    int handler;
	} ccatch;
    } u;
    Error_action_specifier *next;
};

struct handler_info {
    list_t *traceback;
    Ident error;
    Handler_info *next;
};

extern Frame *cur_frame;
extern connection_t *cur_conn;
extern data_t *stack;
extern int stack_pos, stack_size;
extern int *arg_starts, arg_pos, arg_size;
extern string_t *numargs_str;
extern long task_id;
extern long tick;
extern VMState *paused;

void init_execute(void);
void task(connection_t *conn, Dbref dbref, long message, int num_args, ...);
void task_method(connection_t *conn, object_t *obj, method_t *method);
long frame_start(object_t *obj, method_t *method, Dbref sender, Dbref caller,
		 int stack_start, int arg_start);
void frame_return(void);
void anticipate_assignment(void);
Ident pass_message(int stack_start, int arg_start);
Ident send_message(Dbref dbref, Ident message, int stack_start, int arg_start);
void pop(int n);
void check_stack(int n);
void push_int(long n);
void push_float(float f);
void push_string(string_t *str);
void push_dbref(Dbref dbref);
void push_list(list_t *list);
void push_symbol(Ident id);
void push_error(Ident id);
void push_dict(Dict *dict);
void push_buffer(Buffer *buffer);
int func_init_0(void);
int func_init_1(data_t **args, int type1);
int func_init_2(data_t **args, int type1, int type2);
int func_init_3(data_t **args, int type1, int type2, int type3);
int func_init_0_or_1(data_t **args, int *num_args, int type1);
int func_init_1_or_2(data_t **args, int *num_args, int type1, int type2);
int func_init_2_or_3(data_t **args, int *num_args, int type1, int type2,
		     int type3);
int func_init_1_to_3(data_t **args, int *num_args, int type1, int type2,
		     int type3);
void func_num_error(int num_args, char *required);
void func_type_error(char *which, data_t *wrong, char *required);
/* void func_error(Ident id, char *fmt, ...); */
void cthrow(long id, char *fmt, ...);
void unignorable_error(Ident id, string_t *str);
void interp_error(Ident error, string_t *str);
void user_error(Ident error, string_t *str, data_t *arg);
void propagate_error(list_t *traceback, Ident error);
void pop_error_action_specifier(void);
void pop_handler_info(void);
void task_suspend(void);
void task_resume(long tid, data_t *ret);
void task_cancel(long tid);
void task_pause(void);
VMState *task_lookup(long tid);
list_t *task_list(void);
list_t *task_callers(void);
void run_paused_tasks(void);
void bind_opcode(int opcode, Dbref dbref);

#endif

