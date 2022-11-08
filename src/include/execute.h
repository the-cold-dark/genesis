/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef execute_h
#define execute_h

typedef struct frame Frame;
typedef struct error_action_specifier Error_action_specifier;
typedef struct handler_info Handler_info;
typedef struct traceback_info Traceback_info;
typedef struct vmstate VMState;
typedef struct vmstack VMStack;
typedef struct task_s task_t;

#include <sys/types.h>
#include <stdarg.h>

/* We use the MALLOC_DELTA defines to keep table sizes thirty-two bytes less
 * than a power of two, which is helpful on buddy systems. */
#define STACK_MALLOC_DELTA 4
#define ARG_STACK_MALLOC_DELTA 8

struct vmstack {
    cData   * stack;
    Int       stack_size,
            * arg_starts,
              arg_size;
    VMStack * next;
};

struct vmstate {
    Frame   * cur_frame;
    cData   * stack;
    Int       stack_pos,
              stack_size,
            * arg_starts,
              arg_pos,
              arg_size;
    Int       task_id;
    Int       frame_depth;
    Int       preempted;
#ifdef DRIVER_DEBUG
    cData     debug;
#endif
    Int       limit_datasize;
    Int       limit_fork;
    Int       limit_calldepth;
    Int       limit_recursion;
    Int       limit_objswap;
    VMState * next;
};

struct task_s {
    cObjnum   objnum;
    Ident      method;
    Int        stack_start;
    Int        arg_start;
    task_t   * next;
};

struct frame {
    Obj *object;
    cObjnum sender;
    cObjnum caller;
    cObjnum user;
    Method *method;
    Long *opcodes;
    Int pc;
    Int last_opcode;
    Int ticks;
    Int stack_start;
    Int var_start;
    IsFrob is_frob;
    Error_action_specifier *specifiers;
    Handler_info *handler_info;
    Frame *caller_frame;
};

struct error_action_specifier {
    Int type;
    Int stack_pos;
    Int arg_pos;
    union {
        struct {
            Int end;
        } critical;
        struct {
            Int end;
        } propagate;
        struct {
            Int error_list;
            Int handler;
        } ccatch;
    } u;
    Error_action_specifier *next;
};

/*
 *  * type specifies interp_error, user_error or
 *   */
struct traceback_info {
    Int              type;
    Ident            location;
    union {
        cData          * method_name;
        Ident            opcode;
    } u;
    cObjnum          current_obj;
    cObjnum          defining_obj;
    Method         * method;
    Int              pc;
    Traceback_info * next;
};


struct handler_info {
    cList          * cached_traceback;
    Traceback_info * traceback;
    Ident            error;
    cStr           * error_message;
    cData          * error_data;
    Handler_info   * next;
};

#define MF_NONE      0    /* No flags */
#define MF_NOOVER    1    /* not overridable */
#define MF_SYNC      2    /* synchronized */
#define MF_LOCK      4    /* locked */
#define MF_NATIVE    8    /* native */
#define MF_FORK      16   /* fork */
#define MF_UNDF2     32   /* undefined */
#define MF_UNDF3     64   /* undefined */
#define MF_UNDF4     128  /* undefined */

/* define these separately, so we can switch the result of 'call_method' */
#define    CALL_OK       0
#define    CALL_NATIVE   1
#define    CALL_FORK     2
#define    CALL_ERROR    3

#define    CALL_ERR_NONE     0
#define    CALL_ERR_NUMARGS  1
#define    CALL_ERR_MAXDEPTH 2
#define    CALL_ERR_OBJNF    3
#define    CALL_ERR_METHNF   4
#define    CALL_ERR_PRIVATE  5
#define    CALL_ERR_PROT     6
#define    CALL_ERR_ROOT     7
#define    CALL_ERR_DRIVER   8

extern Frame *cur_frame;
extern cData *stack;
extern Int stack_pos, stack_size;
extern Int *arg_starts, arg_pos, arg_size;
extern cStr *numargs_str;
extern Long task_id;
extern Long call_environ;
extern Long tick;
extern VMState * preempted;
extern VMState * suspended;

void init_execute(void);
void uninit_execute(void);
void vm_task(cObjnum objnum, Ident name, Int num_args, ...);
void vm_method(Obj *obj, Method *method);
Int  frame_start(Obj *obj,
                 Method *method,
                 cObjnum sender,
                 cObjnum caller,
                 cObjnum user,
                 Int stack_start,
                 Int arg_start,
                 IsFrob is_frob);
void pop_native_stack(Int start);
void frame_return(void);
void anticipate_assignment(void);
Int pass_method(Int stack_start, Int arg_start);
Int call_method(cObjnum objnum, Ident message, Int stack_start, Int arg_start, IsFrob is_frob);
void pop(Int n);
void check_stack(Int n);
Traceback_info *traceback_info_dup(Traceback_info *info);

#define F_PUSH(_name_, _c_type_) \
    void CAT(push_, _name_) (_c_type_ var)
#define N_PUSH(_name_, _c_type_) \
    void CAT(native_push_, _name_) (_c_type_ var)

F_PUSH(int,    cNum);
F_PUSH(float,  cFloat);
F_PUSH(string, cStr *);
F_PUSH(objnum, cObjnum);
F_PUSH(list,   cList *);
F_PUSH(dict,   cDict *);
F_PUSH(symbol, Ident);
F_PUSH(error,  Ident);
F_PUSH(buffer, cBuf *);

N_PUSH(int,    cNum);
N_PUSH(float,  cFloat);
N_PUSH(string, cStr *);
N_PUSH(objnum, cObjnum);
N_PUSH(list,   cList *);
N_PUSH(dict,   cDict *);
N_PUSH(symbol, Ident);
N_PUSH(error,  Ident);
N_PUSH(buffer, cBuf *);

#undef F_PUSH
#undef N_PUSH

Int func_init_0(void);
Int func_init_1(cData **args, Int type1);
Int func_init_2(cData **args, Int type1, Int type2);
Int func_init_3(cData **args, Int type1, Int type2, Int type3);
Int func_init_0_or_1(cData **args, Int *num_args, Int type1);
Int func_init_0_to_2(cData **args, Int *num_args, Int type1, Int type2);
Int func_init_1_or_2(cData **args, Int *num_args, Int type1, Int type2);
Int func_init_2_or_3(cData **args, Int *num_args, Int type1, Int type2,
                     Int type3);
Int func_init_3_or_4(cData **args, Int *num_args, Int type1, Int type2,
                     Int type3, Int type4);
Int func_init_1_to_3(cData **args, Int *num_args, Int type1, Int type2,
                     Int type3);
void func_num_error(Int num_args, const char *required);
void func_type_error(const char *which, cData *wrong, const char *required);
void cthrow(Ident id, const char *fmt, ...);
void unignorable_error(Ident id, cStr *str);
void interp_error(Ident error, cStr *str);
void user_error(Ident error, cStr *str, cData *arg);
void propagate_error(Traceback_info *traceback, Ident error,
                     cStr *explnation, cData *arg);
void pop_error_action_specifier(void);
void pop_handler_info(void);
cList *generate_traceback(Traceback_info *traceback);

void      vm_suspend(void);
cList   * vm_info(Long tid);
void      vm_resume(Long tid, cData *ret);
void      vm_cancel(Long tid);
void      vm_pause(void);
VMState * vm_lookup(Long tid);
cList   * vm_list(void);
cList   * vm_stack(Frame * frame_to_trace, bool calculate_line_numbers);
void      log_task_stack(Long taskid, cList * stack,
                         void (logroutine)(const char*,...));
void      run_paused_tasks(void);
void      bind_opcode(Int opcode, cObjnum objnum);
VMState * vm_current(void);

#ifdef DRIVER_DEBUG
void init_debug(void);
void clear_debug(void);
void start_debug(void);
void start_full_debug(void);
void get_debug (cData *d);
#endif

#ifdef PROFILE_EXECUTE
void dump_execute_profile(void);
#endif

#define INVALID_BINDING \
    (op_table[cur_frame->last_opcode].binding != INV_OBJNUM && \
     op_table[cur_frame->last_opcode].binding != \
     cur_frame->method->object->objnum)
#define FUNC_NAME() (op_table[cur_frame->last_opcode].name)
#define FUNC_BINDING() (op_table[cur_frame->last_opcode].binding)

#include "macros.h"

#endif

