/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Routines for executing ColdC tasks.
*/

#include "defs.h"

#include <stdarg.h>
#include <ctype.h>
#include "cdc_pcode.h"
#include "cache.h"
#include "util.h"
#include "moddef.h"

#define STACK_STARTING_SIZE                (256 - STACK_MALLOC_DELTA)
#define ARG_STACK_STARTING_SIZE                (32 - ARG_STACK_MALLOC_DELTA)

extern Bool running;

INTERNAL void execute(void);
INTERNAL void out_of_ticks_error(void);
INTERNAL void start_error(Ident error, cStr *explanation, cData *arg,
                        cList * location);
INTERNAL cList * traceback_add(cList * traceback, Ident error);
INTERNAL void fill_in_method_info(cData *d);

INTERNAL Frame *frame_store = NULL;
INTERNAL Int frame_depth;
cStr *numargs_str;

Frame *cur_frame, *suspend_frame;
cData * stack;
Int stack_pos, stack_size;
Int *arg_starts, arg_pos, arg_size;
Long task_id=1;
Long next_task_id=2;
Long call_environ=1;
Long tick;

#define DEBUG_VM DISABLED
#define DEBUG_EXECUTE DISABLED

void clear_debug(void);
cData debug;

VMState *suspended = NULL, *preempted = NULL, *vmstore = NULL;
VMStack *stack_store = NULL, *holder_cache = NULL; 

#define    call_error(_err_) { call_environ = _err_; return CALL_ERROR; }

/*
// ---------------------------------------------------------------
//
// These two defines add and remove tasks from task lists.
//
*/
#define ADD_VM_TASK(the_list, the_value) { \
        if (!the_list) { \
            the_list = the_value; \
            the_value->next = NULL; \
        } else { \
            the_value->next = the_list; \
            the_list = the_value; \
        } \
    }

#define REMOVE_VM_TASK(the_list, the_value) { \
        if (the_list == the_value) { \
            the_list = the_list->next; \
        } else { \
            vm_delete(the_list, the_value); \
        } \
    }

/*
// ---------------------------------------------------------------
*/
void store_stack(void) {
    VMStack * holder;

    if (holder_cache) {
        holder = holder_cache;
        holder_cache = holder_cache->next;
    } else {
        holder = EMALLOC(VMStack, 1);
    }
  
    holder->stack = stack;
    holder->stack_size = stack_size;

    holder->arg_starts = arg_starts;
    holder->arg_size = arg_size;

    holder->next = stack_store;
    stack_store = holder;
}

/*
// ---------------------------------------------------------------
*/
VMState * vm_current(void) {
    VMState * vm;

    if (vmstore) {
        vm = vmstore;
        vmstore = vmstore->next;
    } else {
        vm = EMALLOC(VMState, 1);
    }

    vm->preempted = NO;
    vm->cur_frame = cur_frame;
    vm->stack = stack;
    vm->stack_pos = stack_pos;
    vm->stack_size = stack_size;
    vm->arg_starts = arg_starts;
    vm->arg_pos = arg_pos;
    vm->arg_size = arg_size;
    vm->task_id = task_id;
    vm->frame_depth = frame_depth;
    vm->next = NULL;
    vm->limit_datasize = limit_datasize;
    vm->limit_fork = limit_fork;
    vm->limit_recursion = limit_recursion;
    vm->limit_objswap = limit_objswap;
    vm->limit_calldepth = limit_calldepth;

#ifdef DRIVER_DEBUG
    data_dup(&vm->debug, &debug);
#endif

    return vm;
}

/*
// ---------------------------------------------------------------
*/
void restore_vm(VMState *vm) {
    task_id = vm->task_id;
    frame_depth = vm->frame_depth;
    cur_frame = vm->cur_frame;
    stack = vm->stack;
    stack_pos = vm->stack_pos;
    stack_size = vm->stack_size;
    arg_starts = vm->arg_starts;
    arg_pos = vm->arg_pos;
    arg_size = vm->arg_size;
    limit_datasize = vm->limit_datasize;
    limit_fork = vm->limit_fork;
    limit_recursion = vm->limit_recursion;
    limit_objswap = vm->limit_objswap;
    limit_calldepth = vm->limit_calldepth;

#ifdef DRIVER_DEBUG
    data_discard(&debug);
    debug = vm->debug;
    vm->debug.type = INTEGER;
    vm->debug.u.val = 0;
#endif

#if DEBUG_VM
    write_err("restore_vm: tid %d opcode %s",
              vm->task_id, op_table[cur_frame->opcodes[cur_frame->pc]].name);
#endif
}


/*
// ---------------------------------------------------------------
*/
void vm_delete(VMState *list, VMState *elem) {
    while (list && (list->next != elem))
        list = list->next;
    if (list)
        list->next = elem->next;
}

/*
// ---------------------------------------------------------------
*/
VMState *vm_lookup(Long tid) {
    VMState * vm;

    for (vm = suspended;  vm;  vm = vm->next)
        if (vm->task_id == tid)
            return vm;

    for (vm = preempted;  vm;  vm = vm->next)
        if (vm->task_id == tid)
            return vm;

    return NULL;
}

/*
// ---------------------------------------------------------------
// dump info on an entire task
//
*/

cList * frame_info(Frame * frame) {
    cList * list;
    cData   d;

    if (!frame)
        return NULL;

    list = list_new(8);

    d.type = OBJNUM;
    d.u.objnum = frame->object->objnum;
    list = list_add(list, &d);
    d.u.objnum = frame->caller;
    list = list_add(list, &d);
    d.u.objnum = frame->sender;
    list = list_add(list, &d);
    d.u.objnum = frame->user;
    list = list_add(list, &d);
    d.type = INTEGER;
    d.u.val = frame->pc;
    list = list_add(list, &d);
    d.u.val = frame->last_opcode;
    list = list_add(list, &d);
    d.u.val = frame->ticks;
    list = list_add(list, &d);
    d.type = SYMBOL;
    d.u.symbol = frame->method->name;
    list = list_add(list, &d);

    return list;
}

cList * vm_info(Long tid) {
    cList   * list;
    Frame   * frame;
    cData     d,
            * dl;
    VMState * vm = vm_lookup(tid);

    if (!vm)
        return NULL;

    list = list_new(2);

    d.type = LIST;
    d.u.list = list_new(7);
    dl = list_empty_spaces(d.u.list, 7);

    /* ARG[1] == task_id */
    dl[0].type = INTEGER;
    dl[0].u.val = vm->task_id;

    /* ARG[2] == preempted? */
    dl[1].type = INTEGER;
    dl[1].u.val = vm->preempted;

    /* ARG[3..7] == limit_datasize */
    dl[2].type = INTEGER;
    dl[2].u.val = vm->limit_datasize;
    dl[3].type = INTEGER;
    dl[3].u.val = vm->limit_fork;
    dl[4].type = INTEGER;
    dl[4].u.val = vm->limit_recursion;
    dl[5].type = INTEGER;
    dl[5].u.val = vm->limit_objswap;
    dl[6].type = INTEGER;
    dl[6].u.val = vm->limit_calldepth;

    /* frames */
    list = list_add(list, &d);
    list_discard(d.u.list);
    d.type = LIST;
    frame = vm->cur_frame;
    while (frame) {
        d.u.list = frame_info(frame);
        list = list_add(list, &d);
        list_discard(d.u.list);
        frame = frame->caller_frame;
    }

    return list;
}

/*
// ---------------------------------------------------------------
// we assume tid is a non-preempted task
//
*/
void vm_resume(Long tid, cData *ret) {
    VMState * vm = vm_lookup(tid),
            * old_vm;

    if (vm->task_id == task_id)
        return;
    old_vm = vm_current();
    restore_vm(vm);
    REMOVE_VM_TASK(suspended, vm);
    ADD_VM_TASK(vmstore, vm);
    if (ret) {
        check_stack(1);
        data_dup(&stack[stack_pos], ret);
        stack_pos++;
    } else {
        push_int(0);
    }
    if (cur_frame->ticks < PAUSED_METHOD_TICKS)
        cur_frame->ticks = PAUSED_METHOD_TICKS;
    execute();
    store_stack();
    restore_vm(old_vm);
    ADD_VM_TASK(vmstore, old_vm);
}

/*
// ---------------------------------------------------------------
*/
Int fork_method(Obj * obj,
                Method * method,
                cObjnum    sender,
                cObjnum    caller,
                cObjnum    user,
                Int      stack_start,
                Int      arg_start,
                Bool     is_frob)
{
    VMState * current = vm_current();
    Int       count, spos, result;

    /* get a new execution environment */
    init_execute();
    cur_frame = NULL;
    task_id = next_task_id++;
    cache_grab(obj);
    cache_grab(method->object);
    method_dup(method);

    /* dup the call method args from the original stack */
    count = current->stack_pos - stack_start;
    spos = stack_start;
    check_stack(count);
    while (count--)
        data_dup(&stack[stack_pos++], &current->stack[spos++]);

    result = frame_start(obj, method, sender, caller, user,
                         0, arg_start - stack_start, is_frob);

    if (result == CALL_ERROR) {
        /* we errored out, clean up the stack */
        pop(stack_pos);
    } else {
        /* pause it, and let system handle it later, as a normal paused task */
        vm_pause();
        result = CALL_FORK;
        call_environ = task_id;
    }
    store_stack();

    cache_discard(method->object);
    method_discard(method);
    cache_discard(obj);

    restore_vm(current);
    ADD_VM_TASK(vmstore, current);

    /* clean up the stack */
    if (result != CALL_ERROR) {
        pop(stack_pos - stack_start);
        push_int(call_environ);
    }

    return result;
}

/*
// ---------------------------------------------------------------
*/
void vm_suspend(void) {
    VMState * vm = vm_current();

    ADD_VM_TASK(suspended, vm);
    init_execute();
    cur_frame = NULL;
}

#ifdef REF_COUNT_DEBUG
void dump_stack (void) {
    Frame *f = cur_frame;

    while (f) {
        printf("user #%d, sender #%d, caller #%d, #%d<#%d>.%s (%d)\n",
               f->user, f->sender, f->caller, f->object->objnum,
               f->method->object->objnum, ident_name(f->method->name),
               f->method->refs);
        f = f->caller_frame;
    }
    printf ("---\n");
}

/* This call counts the references from the stack frames to the given object */
int count_stack_refs (int objnum) {
    Frame *f = cur_frame;
    int s;

    s=0;
    while (f) {
        if (f->object->objnum == objnum)
            s++;
        if (f->method->object->objnum == objnum)
            s++;
        f = f->caller_frame;
    }
    return s;
}
#endif

/*
// ---------------------------------------------------------------
// Nothing calls this function - it's here as a VM debug utility
*/
#if DISABLED
void show_queues(void) {
    VMState * v;

    fputs("preempted:", errfile);
    for (v=preempted; v; v=v->next)
        fprintf(errfile, "%x ", v);
    fputs("\nsuspended:", errfile);
    for (v=suspended; v; v=v->next)
        fprintf(errfile, "%x ", v);
    fputs("\nvmstore:", errfile);
    for (v=vmstore; v; v=v->next)
        fprintf(errfile, "%x ", v);
    fputs("\n\n", errfile);
    fflush(errfile);
}
#endif

/*
// ---------------------------------------------------------------
*/
void vm_cancel(Long tid) {
    VMState * vm = vm_lookup(tid),
            * old_vm;

    if (vm == NULL) {
        write_err("ACK:  Tried to cancel an invalid task id.");
        return;
    }

    if (task_id == vm->task_id) {
        old_vm = NULL;
    } else {
        old_vm = vm_current();
        restore_vm(vm);
    }
    while (cur_frame)
        frame_return();
    if (old_vm != NULL) {
        if (vm->preempted)
            REMOVE_VM_TASK(preempted, vm)
        else
            REMOVE_VM_TASK(suspended, vm)
        store_stack();
        ADD_VM_TASK(vmstore, vm);
        restore_vm(old_vm);
        ADD_VM_TASK(vmstore, old_vm);
    }
}

/*
// ---------------------------------------------------------------
*/
void vm_pause(void) {
    VMState * vm = vm_current();

    vm->preempted = YES;
    ADD_VM_TASK(preempted, vm);
    init_execute();
    cur_frame = NULL;  
}

/*
// ---------------------------------------------------------------
*/
void run_paused_tasks(void) {
    VMState * vm = vm_current(),
            * task = preempted,
            * last_task;

    /* tasks preempting again will be on a new list */
    preempted = NULL;

    while (task) {
        restore_vm(task);
        cur_frame->ticks = PAUSED_METHOD_TICKS;
        last_task = task;
        task = task->next;
        ADD_VM_TASK(vmstore, last_task);
        execute();
        store_stack();
    }

    restore_vm(vm);
    ADD_VM_TASK(vmstore, vm);
}

/*
// ---------------------------------------------------------------
//
// List tasks
//
*/

cList * vm_list(void) {
    cList  * r;
    cData    elem;
    VMState * vm;
  
    r = list_new(0);
  
    elem.type = INTEGER;

    for (vm = suspended; vm; vm = vm->next) {
        elem.u.val = vm->task_id;
        r = list_add(r, &elem); 
    }
  
    for (vm = preempted; vm; vm = vm->next) {
        elem.u.val = vm->task_id;
        r = list_add(r, &elem); 
    }
  
    return r;
}

/*
// ---------------------------------------------------------------
*/
cList * vm_stack(void) {
    cList * r;
    cData   d,
           * list;
    Frame  * f;
  
    r = list_new(0);
    d.type = LIST;
    for (f = cur_frame; f; f = f->caller_frame) {

        d.u.list = list_new(5);
        list = list_empty_spaces(d.u.list, 5);

        list[0].type = OBJNUM;
        list[0].u.objnum = f->object->objnum;
        list[1].type = OBJNUM;
        list[1].u.objnum = f->method->object->objnum;
        list[2].type = SYMBOL;
        list[2].u.symbol = ident_dup(f->method->name);
        list[3].type = INTEGER;
        list[3].u.val = line_number(f->method, f->pc - 1);
        list[4].type = INTEGER;
        list[4].u.val = (Long) f->pc;

        r = list_add(r, &d);
        list_discard(d.u.list);
    }

    return r;
}

/*
// ---------------------------------------------------------------
*/
void init_execute(void) {
    if (stack_store) {
        VMStack *holder;
        
        stack = stack_store->stack;
        stack_size = stack_store->stack_size;
    
        arg_starts = stack_store->arg_starts;
        arg_size = stack_store->arg_size;
    
        holder = stack_store;
        stack_store = holder->next;
        holder->next = holder_cache;
        holder_cache = holder;

#if DEBUG_VM
        write_err("resuing execution state");
#endif

    } else {
        stack = EMALLOC(cData, STACK_STARTING_SIZE);
        stack_size = STACK_STARTING_SIZE;
    
        arg_starts = EMALLOC(Int, ARG_STACK_STARTING_SIZE);
        arg_size = ARG_STACK_STARTING_SIZE;

#if DEBUG_VM
        write_err("allocating execution state");
#endif

    }
    stack_pos = 0;
    arg_pos = 0;
    frame_depth = 0;

    /* reset limits */
    limit_datasize = 0;
    limit_fork = 0;
    limit_recursion = 128;
    limit_objswap = 0;
    limit_calldepth = 128;

#ifdef DRIVER_DEBUG
    clear_debug();
#endif
}

/*
// ---------------------------------------------------------------
//
// Execute a task, if we are currently executing, preempt the current
// task, we get priority.
//
// No we dont, lets just rewrite the interpreter, this sucks.
*/
void vm_task(cObjnum objnum, Long name, Int num_args, ...) {
    va_list arg;

    /* Don't execute if a shutdown() has occured. */
    if (!running) {
        va_end(arg);
        return;
    }

    /* Set global variables. */
    frame_depth = 0;
    clear_debug();

    va_start(arg, num_args);
    check_stack(num_args);
    while (num_args--)
        data_dup(&stack[stack_pos++], va_arg(arg, cData *));
    va_end(arg);

    /* start the task */
    ident_dup(name);
    if (call_method(objnum, name, 0, 0, FROB_NO) == CALL_ERROR) {
        pop(stack_pos);
    } else {
        execute();
        if (stack_pos != 0) {
            int x;
            write_err("PANIC: Stack not empty after interpretation (%d):",
                      stack_pos);
            for (x=0; x <= stack_pos; x++)
                write_err("PANIC:     stack[%d] => %D", x, &stack[x]);
            panic("Attempting clean shutdown.");
        }
        task_id = next_task_id++;
    }
    ident_discard(name);
}

/*
// ---------------------------------------------------------------
//
// Execute a task by evaluating a method on an object.
//
*/
void vm_method(Obj *obj, Method *method) {
    clear_debug();
    frame_start(obj, method, NOT_AN_IDENT, NOT_AN_IDENT, NOT_AN_IDENT, 0, 0, FROB_NO);

    execute();

    if (stack_pos != 0) {
        int x;
        write_err("PANIC: Stack not empty after interpretation:");
        for (x=0; x <= stack_pos; x++)
            write_err("PANIC:     stack[%d] => %D", x, &stack[x]);
        panic("Attempting clean shutdown.");
    }
}

/*
// ---------------------------------------------------------------
*/
Int frame_start(Obj * obj,
                Method * method,
                cObjnum    sender,
                cObjnum    caller,
                cObjnum    user,
                Int      stack_start,
                Int      arg_start,
		Bool     is_frob)
{
    Frame      * frame;
    Int          i,
                 num_args,
                 num_rest_args;
    cList     * rest;
    cData       * d, o;
    Number_buf   nbuf1,
                 nbuf2;

    num_args = stack_pos - arg_start;
    if (num_args < method->num_args || (num_args > method->num_args &&
                                        method->rest == -1)) {
        if (numargs_str)
            string_discard(numargs_str);
        o.type = OBJNUM;
        o.u.objnum = obj->objnum;
        numargs_str = format("%D.%s() called with %s argument%s, requires %s%s",
                             &o, ident_name(method->name),
                             english_integer(num_args, nbuf1),
                             (num_args == 1) ? "" : "s",
                             (method->num_args == 0) ? "none" :
                             english_integer(method->num_args, nbuf2),
                             (method->rest == -1) ? "." : " or more.");
        call_error(CALL_ERR_NUMARGS)
    }

    if (frame_depth > limit_calldepth)
        call_error(CALL_ERR_MAXDEPTH);
    frame_depth++;

    if (method->rest != -1) {
        /* Make a list for the remaining arguments. */
        num_rest_args = stack_pos - (arg_start + method->num_args);
        rest = list_new(num_rest_args);

        /* Move aforementioned remaining arguments into the list. */
        d = list_empty_spaces(rest, num_rest_args);
        MEMCPY(d, &stack[stack_pos - num_rest_args], num_rest_args);
        stack_pos -= num_rest_args;

        /* Push the list onto the stack. */
        push_list(rest);
        list_discard(rest);
    }

    if (frame_store) {
        frame = frame_store;
        frame_store = frame_store->caller_frame;
    } else {
        frame = EMALLOC(Frame, 1);
    }

    frame->object = cache_grab(obj);
    frame->sender = sender;
    frame->caller = caller;
    frame->user = user;
    frame->method = method_dup(method);
    cache_grab(method->object);
    frame->opcodes = method->opcodes;
    frame->pc = 0;
    frame->ticks = METHOD_TICKS;

    frame->specifiers = NULL;
    frame->handler_info = NULL;
    frame->is_frob=is_frob;

    /* Set up stack indices. */
    frame->stack_start = stack_start;
    frame->var_start = arg_start;

    /* Initialize local variables to 0. */
    check_stack(method->num_vars);
    for (i = 0; i < method->num_vars; i++) {
        stack[stack_pos + i].type = INTEGER;
        stack[stack_pos + i].u.val = 0;
    }
    stack_pos += method->num_vars;

    frame->caller_frame = cur_frame;
    cur_frame = frame;

#ifdef DRIVER_DEBUG
    if (debug.u.val > 0) {
      Int parms;
      cList *list;  
      cData d; 

      parms = (debug.u.val == 2 ||
               (debug.u.val >= 4 &&
                list_length(list_elem(debug.u.list,0)->u.list) == 5));

      if (debug.type != LIST) {
          debug.type = LIST;
          debug.u.list = list_new(256);
      }

      list = list_new(4);
      d.type=INTEGER;
      d.u.val = tick;
      list = list_add(list, &d);
      d.type = OBJNUM;
      d.u.objnum = frame->object->objnum;
      list = list_add(list, &d);
      d.type = OBJNUM;
      d.u.objnum = method->object->objnum;
      list = list_add(list, &d);
      d.type = SYMBOL;
      d.u.symbol = ident_dup(method->name);
      list = list_add(list, &d);
      ident_discard(method->name);

      if (parms) {
          cList *l;
          Int i; 

          l = list_new(1);
          for (i = arg_start; i < stack_pos - method->num_vars; i++)
              l = list_add(l, &stack[i]);
          d.type = LIST;
          d.u.list = l;
          list = list_add(list, &d);
          list_discard(l);
      }
      d.type = LIST;
      d.u.list = list;
      debug.u.list = list_add(debug.u.list, &d);
      list_discard(list);
    }           
#endif

    return CALL_OK;
}

/*
// ---------------------------------------------------------------
*/
void frame_return(void) {
    Int i;
    Frame *caller_frame = cur_frame->caller_frame;

#ifdef DRIVER_DEBUG
    if (debug.u.val > 0) {
      cData d;
    
      if (debug.type == LIST) {
          /* We skip the case when there hasn't been any calls yet,
             That's to prefent the other routine from getting confused */
          d.type = INTEGER;
          d.u.val = tick;
          debug.u.list = list_add (debug.u.list, &d);
      }   
    }     
#endif    

    /* Free old data on stack. */
    for (i = cur_frame->stack_start; i < stack_pos; i++)
        data_discard(&stack[i]);
    stack_pos = cur_frame->stack_start;

    /* Let go of method and objects. */
    
#ifdef REF_COUNT_DEBUG
    /* Check if any of the objects lost their refcounts */
    if (count_stack_refs(cur_frame->method->object->objnum) >
        cur_frame->method->object->refs)
    {
        printf ("EErrp!\n");
        fflush(stdout);
    }
#endif

    cache_discard(cur_frame->method->object);
    method_discard(cur_frame->method);
    cache_discard(cur_frame->object);

    /* Discard any error action specifiers. */
    while (cur_frame->specifiers)
        pop_error_action_specifier();

    /* Discard any handler information. */
    while (cur_frame->handler_info)
        pop_handler_info();

    /* Append frame to frame store for later reuse. */
    cur_frame->caller_frame = frame_store;
    frame_store = cur_frame;

    /* Return to the caller frame. */
    cur_frame = caller_frame;

    frame_depth--;
}

#ifdef PROFILE_EXECUTE

Int meth_p_last = 0;

struct meth_prof_s {
    cObjnum objnum;
    char     name[64];
    uLong    count;
} meth_prof [PROFILE_MAX];

Long prof_ops[LAST_TOKEN];

void update_execute_opcode(Int opcode) {
    register Int x;
    static short init = 1;
        
    if (init) {
        for (x=0; x < LAST_TOKEN; x++)
            prof_ops[x] = 0;
        init = 0;
    }       

    prof_ops[opcode]++;
}

void update_execute_method(Method * method) {
    register Int x;
    register char * name, * c;
    register cObjnum obj;

    if (method->name == NOT_AN_IDENT)
        return;

    name = ident_name(method->name);
    obj  = method->object->objnum;

    for (x=0; x <= meth_p_last; x++) {
        if (meth_prof[x].objnum == obj && !strcmp(meth_prof[x].name, name)) {
            meth_prof[x].count++;
            return;
        }
    }

    if (meth_p_last == (PROFILE_MAX - 1)) {
        dump_execute_profile();
        meth_p_last = 0;
    }

    meth_p_last++;
    meth_prof[meth_p_last].objnum = obj;
    c = meth_prof[meth_p_last].name;

    strcpy(meth_prof[meth_p_last].name, name);

    meth_prof[meth_p_last].count = 1;

}

void dump_execute_profile(void) {
    register Int x;
    cStr * str;
    cData d;

    fputs("Opcodes:\n", errfile);
    for (x=0; x < LAST_TOKEN; x++) {
        if (prof_ops[x])
            fprintf(errfile, "  %-10ld %-5d %s\n",
                    prof_ops[x], x, op_table[x].name);
    }

    d.type = OBJNUM;
    fputs("Methods:\n", errfile);
    for (x=0; x < meth_p_last; x++) {
        d.u.objnum = meth_prof[x].objnum;
        str = data_to_literal(&d, DF_WITH_OBJNAMES);
        fprintf(errfile, "  %-10ld %s.%s\n",
                meth_prof[x].count, string_chars(str), meth_prof[x].name);
        string_discard(str);
    }
}

#endif

/*
// ---------------------------------------------------------------
*/
#ifdef USE_BIG_NUMBERS
#define MAX_NUM 2147483647
#else
#define MAX_NUM 2147483647
#endif

INTERNAL void execute(void) {
    Int opcode;

    while (cur_frame) {
        if (tick == MAX_NUM)
            tick = -1;
        tick++;
        if ((--(cur_frame->ticks)) == 0) {
            out_of_ticks_error();
        } else {
            opcode = cur_frame->opcodes[cur_frame->pc];

#if DEBUG_EXECUTE
            fprintf(errfile, "<==> %d %s ",
                    line_number(cur_frame->method, cur_frame->pc),
                    op_table[cur_frame->opcodes[cur_frame->pc]].name);
            write_err("%O.%I",
                cur_frame->method->object->objnum,
                ((cur_frame->method->name != NOT_AN_IDENT) ?
                    cur_frame->method->name :
                    opcode_id));
    /*        fflush(errfile); */
#endif

            cur_frame->last_opcode = opcode;
            cur_frame->pc++;

#ifdef PROFILE_EXECUTE
            update_execute_opcode(opcode);
#endif
            (*op_table[opcode].func)();
        }
    }
}

/*
// ---------------------------------------------------------------
//
// Requires cur_frame->pc to be the current instruction.  Do NOT call this
// function if there is any possibility of the assignment failing before the
// current instruction finishes.
//
*/
void anticipate_assignment(void) {
    Int opcode, ind;
    Long id;
    cData *dp, d;
    Int pc=cur_frame->pc;

    /* skip error handling */
    while ((opcode = cur_frame->opcodes[pc]) == CRITICAL_END)
	pc++;

    switch (opcode) {
      case SET_LOCAL:
        /* Zero out local variable value. */
        dp = &stack[cur_frame->var_start +
                    cur_frame->opcodes[pc + 1]];
        data_discard(dp);
        dp->type = INTEGER;
        dp->u.val = 0;
        break;
      case SET_OBJ_VAR:
        /* Zero out the object variable, if it exists. */
        ind = cur_frame->opcodes[pc + 1];
        id = object_get_ident(cur_frame->method->object, ind);
        d.type = INTEGER;
        d.u.val = 0;
        object_assign_var(cur_frame->object, cur_frame->method->object,
                          id, &d);
        break;
    }
}

/*
// ---------------------------------------------------------------
//
// Ok, our stack looks like:
//
//  [ ... | target | arg1 | arg2 | ... ]
//          ^^^^^^-- stack_start
//
// make SURE that native methods are clearly duping their data
*/

#if DISABLED
INTERNAL void
call_native_method(Method * method,
                   Int        stack_start,
                   Int        arg_start,
                   cObjnum   objnum)
{
    cData rval;
    register Int i;

    if ((*natives[method->native].func)(arg_start, &rval)) {
        /* push ALL of the old stack off, including the target and name */
        for (i = stack_start; i < stack_pos; i++)
            data_discard(&stack[i]);

        /* 'pop' the return value back on the stack */
        stack_pos = stack_start;
        stack[stack_pos] = rval;
        stack_pos++;
    }
}
#else
#define call_native_method(method, sstart, astart) \
    (*natives[method->native].func)(sstart, astart)
#endif

/*
// because we want to keep references straight, one oft may want to
// grab the data they want off the stack, dup it for their own copy,
// then pop everything off the stack so references would still be
// one (in the cases that matter)
*/

void pop_native_stack(Int start) {
    register Int i;

    for (i = start; i < stack_pos; i++)
        data_discard(&stack[i]);
    stack_pos = start;
}

/*
// ---------------------------------------------------------------
*/
Int pass_method(Int stack_start, Int arg_start) {
    Method *method;
    Int result;

    if (cur_frame->method->name == -1)
        call_error(CALL_ERR_METHNF);

    /* Find the next method to handle the message. */
    method = object_find_next_method(cur_frame->object->objnum,
                                     cur_frame->method->name,
                                     cur_frame->method->object->objnum,
				     cur_frame->method->m_access == MS_FROB ?
                                     FROB_YES : FROB_NO);
    if (!method) {
	if (cur_frame->method->m_access == MS_FROB) {
	    method = object_find_next_method(cur_frame->object->objnum,
					     cur_frame->method->name,
					     cur_frame->method->object->objnum,
					     FROB_RETRY);
	    if (!method)
		call_error(CALL_ERR_METHNF);
	} else
	    call_error(CALL_ERR_METHNF);
    }

    if (cur_frame) {
        switch (method->m_access) {
            case MS_ROOT:
                if (cur_frame->method->object->objnum != ROOT_OBJNUM) {
                    cache_discard(method->object);
                    call_error(CALL_ERR_ROOT);
                }
                break;
            case MS_DRIVER:
                /* if we are here, there is a current frame,
                   and the driver didn't send this */
                cache_discard(method->object);
                call_error(CALL_ERR_DRIVER);
        }
    }

    /* Start the new frame. */
    if (method->native == -1) {
        if (method->m_flags & MF_FORK)
            result = fork_method(cur_frame->object, method, cur_frame->sender,
                             cur_frame->caller, cur_frame->user, stack_start,
                             arg_start, cur_frame->is_frob);
        else
            result = frame_start(cur_frame->object, method, cur_frame->sender,
                             cur_frame->caller, cur_frame->user, stack_start,
                             arg_start, cur_frame->is_frob);
    } else {
        call_native_method(method, stack_start, arg_start);
        result = CALL_NATIVE;
    }
    cache_discard(method->object);
    return result;
}

/*
// ---------------------------------------------------------------
*/
Int call_method(cObjnum objnum,     /* the object */
                Ident name,         /* the method name */
                Int stack_start,    /* start of the stack .. */
                Int arg_start,      /* start of the args */
		Bool is_frob)       /* how to look it up */
{
    Obj * obj;
    Method * method;
    Int        result;
    cObjnum   sender,
               caller, user;

    /* Get the target object from the cache. */
    obj = cache_retrieve(objnum);
    if (!obj)
        call_error(CALL_ERR_OBJNF);

    /* If we're executing a frob method, treat any method calls to
       this() as if it were a frob call */
    if (cur_frame && cur_frame->method->m_access == MS_FROB &&
        cur_frame->object->objnum == objnum)
        is_frob = FROB_YES;

    /* Find the method to run. */
    method = object_find_method(objnum, name, is_frob);
    if (!method) {
	if (is_frob == FROB_YES) {
	    method = object_find_method(objnum, name, FROB_RETRY);
	    if (!method) {
		cache_discard(obj);
		call_error(CALL_ERR_METHNF);
	    }
	}
	else {
            cache_discard(obj);
            call_error(CALL_ERR_METHNF);
	}
    }

#ifdef PROFILE_EXECUTE
    update_execute_method(method);
#endif

    /*
    // check perms
    //     private:   caller has to be definer
    //     protected: sender has to be this
    //     root:      caller has to be $root
    //     driver:    only I can send to this method
    */
    if (cur_frame) {
        switch (method->m_access) {
            case MS_PRIVATE:
                if (cur_frame->method->object->objnum!=method->object->objnum){
                    cache_discard(obj);
                    cache_discard(method->object);
                    call_error(CALL_ERR_PRIVATE);
                }
                break;
            case MS_PROTECTED:
                if (cur_frame->object->objnum != objnum) {
                    cache_discard(obj);
                    cache_discard(method->object);
                    call_error(CALL_ERR_PROT);
                }
                break;
            case MS_ROOT:
                if (cur_frame->method->object->objnum != ROOT_OBJNUM) {
                    cache_discard(obj);
                    cache_discard(method->object);
                    call_error(CALL_ERR_ROOT);
                }
                break;
            case MS_DRIVER:
                /* if we are here, there is a current frame,
                   and the driver didn't send this */
                cache_discard(obj);
                cache_discard(method->object);
                call_error(CALL_ERR_DRIVER);
        }
    }

    /* Start the new frame. */
    if (method->native == -1) {
        sender = (cur_frame) ? cur_frame->object->objnum : INV_OBJNUM;
        caller = (cur_frame) ? cur_frame->method->object->objnum : INV_OBJNUM;
        user   = (cur_frame) ? cur_frame->user : INV_OBJNUM;
        if (method->m_flags & MF_FORK)
            result = fork_method(obj, method, sender, caller, user,
                                 stack_start, arg_start, is_frob);
        else
            result = frame_start(obj, method, sender, caller, user,
                                 stack_start, arg_start, is_frob);
    } else {
        call_native_method(method, stack_start, arg_start);
        result = CALL_NATIVE;
    }

    cache_discard(obj);
    cache_discard(method->object);

    return result;
}

/*
// ---------------------------------------------------------------
*/
void pop(Int n) {

#ifdef DEBUG
    write_err("pop(%d)", n);
#endif

    while (n--)
        data_discard(&stack[--stack_pos]);
}

/*
// ---------------------------------------------------------------
*/
void check_stack(Int n) {
    while (stack_pos + n > stack_size) {
        stack_size = stack_size * 2 + STACK_MALLOC_DELTA;
        stack = EREALLOC(stack, cData, stack_size);
    }
}

/*
// ---------------------------------------------------------------
*/
#define PUSH_DATA(_x_, _name_, _cold_type_, _c_type_, _member_, _what_) \
void CAT(_x_, _name_) (_c_type_ var) { \
    check_stack(1); \
    stack[stack_pos].type = _cold_type_; \
    stack[stack_pos].u._member_ = _what_; \
    stack_pos++; \
}

#define PUSH_FUNC(_name_, _cold_type_, _c_type_, _member_, _what_) \
    PUSH_DATA(push_, _name_, _cold_type_, _c_type_, _member_, _what_)
#define PUSH_NATIVE(_name_, _cold_type_, _c_type_, _member_) \
    PUSH_DATA(native_push_, _name_, _cold_type_, _c_type_, _member_, var)

PUSH_FUNC(int,    INTEGER, cNum,       val,    var)
PUSH_FUNC(float,  FLOAT,   cFloat,      fval,   var)
PUSH_FUNC(string, STRING,  cStr *, str,    string_dup(var))
PUSH_FUNC(objnum, OBJNUM,  cObjnum,   objnum, var)
PUSH_FUNC(list,   LIST,    cList *,   list,   list_dup(var))
PUSH_FUNC(dict,   DICT,    cDict *,   dict,   dict_dup(var))
PUSH_FUNC(symbol, SYMBOL,  Ident,      symbol, ident_dup(var))
PUSH_FUNC(error,  T_ERROR, Ident,      error,  ident_dup(var))
PUSH_FUNC(buffer, BUFFER,  cBuf *, buffer, buffer_dup(var))

PUSH_NATIVE(int,    INTEGER, cNum,       val)
PUSH_NATIVE(float,  FLOAT,   cFloat,      fval)
PUSH_NATIVE(string, STRING,  cStr *, str)
PUSH_NATIVE(objnum, OBJNUM,  cObjnum,   objnum)
PUSH_NATIVE(list,   LIST,    cList *,   list)
PUSH_NATIVE(dict,   DICT,    cDict *,   dict)
PUSH_NATIVE(symbol, SYMBOL,  Ident,      symbol)
PUSH_NATIVE(error,  T_ERROR, Ident,      error)
PUSH_NATIVE(buffer, BUFFER,  cBuf *, buffer)

/*
// ---------------------------------------------------------------
*/
Int func_init_0(void) {
    Int arg_start = arg_starts[--arg_pos];
    Int num_args = stack_pos - arg_start;

    if (num_args)
        func_num_error(num_args, "none");
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_1(cData **args, Int type1) {
    Int arg_start = arg_starts[--arg_pos];
    Int num_args = stack_pos - arg_start;

    *args = &stack[arg_start];
    if (num_args != 1)
        func_num_error(num_args, "one");
    else if (type1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_2(cData **args, Int type1, Int type2)
{
    Int arg_start = arg_starts[--arg_pos];
    Int num_args = stack_pos - arg_start;

    *args = &stack[arg_start];
    if (num_args != 2)
        func_num_error(num_args, "two");
    else if (type1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (type2 && stack[arg_start + 1].type != type2)
        func_type_error("second", &stack[arg_start + 1], english_type(type2));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_3(cData **args, Int type1, Int type2, Int type3)
{
    Int arg_start = arg_starts[--arg_pos];
    Int num_args = stack_pos - arg_start;

    *args = &stack[arg_start];
    if (num_args != 3)
        func_num_error(num_args, "three");
    else if (type1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (type2 && stack[arg_start + 1].type != type2)
        func_type_error("second", &stack[arg_start + 1], english_type(type2));
    else if (type3 && stack[arg_start + 2].type != type3)
        func_type_error("third", &stack[arg_start + 2], english_type(type3));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_0_or_1(cData **args, Int *num_args, Int type1)
{
    Int arg_start = arg_starts[--arg_pos];

    *args = &stack[arg_start];
    *num_args = stack_pos - arg_start;
    if (*num_args > 1)
        func_num_error(*num_args, "at most one");
    else if (type1 && *num_args == 1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_1_or_2(cData **args, Int *num_args, Int type1, Int type2)
{
    Int arg_start = arg_starts[--arg_pos];

    *args = &stack[arg_start];
    *num_args = stack_pos - arg_start;
    if (*num_args < 1 || *num_args > 2)
        func_num_error(*num_args, "one or two");
    else if (type1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (type2 && *num_args == 2 && stack[arg_start + 1].type != type2)
        func_type_error("second", &stack[arg_start + 1], english_type(type2));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_2_or_3(cData **args, Int *num_args, Int type1, Int type2,
                     Int type3)
{
    Int arg_start = arg_starts[--arg_pos];

    *args = &stack[arg_start];
    *num_args = stack_pos - arg_start;
    if (*num_args < 2 || *num_args > 3)
        func_num_error(*num_args, "two or three");
    else if (type1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (type2 && stack[arg_start + 1].type != type2)
        func_type_error("second", &stack[arg_start + 1], english_type(type2));
    else if (type3 && *num_args == 3 && stack[arg_start + 2].type != type3)
        func_type_error("third", &stack[arg_start + 2], english_type(type3));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_3_or_4(cData **args, Int *num_args,
                     Int type1, Int type2, Int type3, Int type4)
{
    Int arg_start = arg_starts[--arg_pos];

    *args = &stack[arg_start];
    *num_args = stack_pos - arg_start;
    if (*num_args < 3 || *num_args > 4)
        func_num_error(*num_args, "three or four");
    else if (type1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (type2 && stack[arg_start + 1].type != type2)
        func_type_error("second", &stack[arg_start + 1], english_type(type2));
    else if (type3 && stack[arg_start + 2].type != type3)
        func_type_error("third", &stack[arg_start + 2], english_type(type3));
    else if (type4 && *num_args == 4 && stack[arg_start + 3].type != type4)
        func_type_error("third", &stack[arg_start + 3], english_type(type4));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

Int func_init_1_to_3(cData **args, Int *num_args, Int type1, Int type2,
                     Int type3)
{
    Int arg_start = arg_starts[--arg_pos];

    *args = &stack[arg_start];
    *num_args = stack_pos - arg_start;
    if (*num_args < 1 || *num_args > 3)
        func_num_error(*num_args, "one to three");
    else if (type1 && stack[arg_start].type != type1)
        func_type_error("first", &stack[arg_start], english_type(type1));
    else if (type2 && *num_args >= 2 && stack[arg_start + 1].type != type2)
        func_type_error("second", &stack[arg_start + 1], english_type(type2));
    else if (type3 && *num_args == 3 && stack[arg_start + 2].type != type3)
        func_type_error("third", &stack[arg_start + 2], english_type(type3));
    else if (INVALID_BINDING)
        cthrow(perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING());
    else
        return 1;
    return 0;
}

void func_num_error(Int num_args, char *required)
{
    Number_buf nbuf;

    cthrow(numargs_id, "Called with %s argument%s, requires %s.",
          english_integer(num_args, nbuf),
          (num_args == 1) ? "" : "s", required);
}

void func_type_error(char *which, cData *wrong, char *required)
{
    cthrow(type_id, "The %s argument (%D) is not %s.", which, wrong, required);
}

INTERNAL Bool is_critical (void) {
    if (cur_frame
	&& cur_frame->specifiers
	&& cur_frame->specifiers->type==CRITICAL)
	return TRUE;
    return FALSE;
}

void cthrow(Ident error, char *fmt, ...)
{
    cStr    * str;
    va_list   arg;
    Method  * method = NULL;

    if (!is_critical()) {
	va_start(arg, fmt);
	str = vformat(fmt, arg);

	va_end(arg);
    } else
	str = NULL;

    /* protect the method in the current frame, if there is any - I
       have no idea what can call cthrow... This will prevent unexpected
       refcounting bombs during the frame_return sequence */
    if (cur_frame)
        method = method_dup(cur_frame->method);
    interp_error(error, str);
    if (method)
        method_discard(method);
    if (str)
	string_discard(str);
}

void interp_error(Ident error, cStr *explanation)
{
    cList * location;
    Ident location_type;
    cData *d;
    char *opname;

    if (explanation) {
	/* Get the opcode name and decide whether it's a function or not. */
	opname = op_table[cur_frame->last_opcode].name;
	location_type = (islower(*opname)) ? function_id : opcode_id;

	/* Construct a two-element list giving the location. */
	location = list_new(2);
	d = list_empty_spaces(location, 2);

	/* The first element is 'function or 'opcode. */
	d->type = SYMBOL;
	d->u.symbol = ident_dup(location_type);
	d++;

	/* The second element is the symbol for the opcode. */
	d->type = SYMBOL;
	d->u.symbol = ident_dup(op_table[cur_frame->last_opcode].symbol);
    }
    else
	location = NULL;

    start_error(error, explanation, NULL, location);
    if (location)
	list_discard(location);
}

void user_error(Ident error, cStr *explanation, cData *arg)
{
    cList  * location;
    cData  * d;
    Method * method;

    /* Construct a list giving the location. */
    location = list_new(5);
    d = list_empty_spaces(location, 5);

    /* The first element is 'method. */
    d->type = SYMBOL;
    d->u.symbol = ident_dup(method_id);
    d++;

    /* The second through fifth elements are the current method info. */
    fill_in_method_info(d);

    /* Return from the current method, and propagate the error. */
    /* protect the current method, so that strings live long enough */
    method = method_dup(cur_frame->method);
    frame_return();
    start_error(error, explanation, arg, location);
    method_discard(method);
    list_discard(location);
}

INTERNAL void out_of_ticks_error(void)
{
    static cStr *explanation;
    cList  * location;
    cData  * d;
    Method * method;

    /* Construct a list giving the location. */
    location = list_new(5);
    d = list_empty_spaces(location, 5);

    /* The first element is 'interpreter. */
    d->type = SYMBOL;
    d->u.symbol = ident_dup(interpreter_id);
    d++;

    /* The second through fifth elements are the current method info. */
    fill_in_method_info(d);

    /* Don't give the topmost frame a chance to return. */
    method = method_dup(cur_frame->method);
    frame_return();
  
    if (!explanation)
        explanation = string_from_chars("Out of ticks", 12);
    start_error(methoderr_id, explanation, NULL, location);
    method_discard(method);
    list_discard(location);
}

INTERNAL void start_error(Ident error, cStr *explanation, cData *arg,
                        cList * location)
{
    cList * error_condition, *traceback;
    cData *d;

    if (location) {
	/* Construct a three-element list for the error condition. */
	error_condition = list_new(3);
	d = list_empty_spaces(error_condition, 3);

	/* The first element is the error code. */
	d->type = T_ERROR;
	d->u.error = ident_dup(error);
	d++;

	/* The second element is the explanation string. */
	d->type = STRING;
	d->u.str = string_dup(explanation);
	d++;

	/* The third element is the error arg, or 0 if there is none. */
	if (arg) {
	    data_dup(d, arg);
	} else {
	    d->type = INTEGER;
	    d->u.val = 0;
	}

	/* Now construct a traceback, starting as a two-element list. */
	traceback = list_new(2);
	d = list_empty_spaces(traceback, 2);

	/* The first element is the error condition. */
	d->type = LIST;
	d->u.list = error_condition;
	d++;

	/* The second argument is the location. */
	d->type = LIST;
	d->u.list = list_dup(location);
    }
    else
	traceback=NULL;

    /* Start the error propagating.  This consumes traceback. */
    propagate_error(traceback, error);
}

/* Requires:  traceback is a list of lists containing the traceback
 *            information to date.  THIS FUNCTION CONSUMES THE INFORMATION.
 *            id is an error id.  This function accounts for an error id
 *            which is "owned" by a data stack frame that we will
 *            nuke in the course of unwinding the call stack.
 *            str is a string containing an explanation of the error. */
void propagate_error(cList * traceback, Ident error)
{
    Int i, ind, propagate = 0;
    Error_action_specifier *spec;
    Error_list *errors;
    Handler_info *hinfo;

    /* If there's no current frame, drop all this on the floor. */
    if (!cur_frame) {
        list_discard(traceback);
        return;
    }

    /* Add message to traceback. */
    if (traceback)
	traceback = traceback_add(traceback, error);

    /* Look for an appropriate specifier in this frame. */
    for (; cur_frame->specifiers; pop_error_action_specifier()) {

        spec = cur_frame->specifiers;
        switch (spec->type) {

          case CRITICAL:

            /* We're in a critical expression.  Make a copy of the error,
             * since it may currently be living in the region of the stack
             * we're about to nuke. */
            error = ident_dup(error);

            /* Nuke the stack back to where we were at the beginning of the
             * critical expression. */
            pop(stack_pos - spec->stack_pos);

            /* Jump to the end of the critical expression. */
            cur_frame->pc = spec->u.critical.end;

            /* make sure arg_pos is correct */
            arg_pos = spec->arg_pos;

            /* Push the error on the stack, and discard our copy of it. */
            push_error(error);
            ident_discard(error);

            /* Pop this error spec, discard the traceback, and continue
             * processing. */
            pop_error_action_specifier();
            if (traceback)
		list_discard(traceback);
            return;

          case PROPAGATE:

            /* We're in a propagate expression.  Set the propagate flag and
             * keep going. */
            propagate = 1;
            break;

          case CATCH:

            /* We're in a catch statement.  Get the error list index. */
            ind = spec->u.ccatch.error_list;

            /* If the index is -1, this was a 'catch any' statement.
             * Otherwise, check if this error code is in the error list. */
            if (spec->u.ccatch.error_list != -1) {
                errors = &cur_frame->method->error_lists[ind];
                for (i = 0; i < errors->num_errors; i++) {
                    if (errors->error_ids[i] == error)
                        break;
                }

                /* Keep going if we didn't find the error. */
                if (i == errors->num_errors)
                    break;
            }

            /* We catch this error.  Make a handler info structure and push it
             * onto the stack. */
            hinfo = EMALLOC(Handler_info, 1);
            hinfo->traceback = traceback;
            hinfo->error = ident_dup(error);
            hinfo->next = cur_frame->handler_info;
            cur_frame->handler_info = hinfo;

            /* Pop the stack down to where we were at the beginning of the
             * catch statement.  This may nuke our copy of error, but we don't
             * need it any more. */
            pop(stack_pos - spec->stack_pos);

            /* make sure arg_pos is correct */
            arg_pos = spec->arg_pos;

            /* Jump to the handler expression, pop this specifier, and continue
             * processing. */
            cur_frame->pc = spec->u.ccatch.handler;
            pop_error_action_specifier();
            return;

        }
    }

    /* There was no handler in the current frame. */
    frame_return();
    propagate_error(traceback, (propagate) ? error : methoderr_id);
}

INTERNAL cList * traceback_add(cList * traceback, Ident error)
{
    cList * frame;
    cData *d, frame_data;

    /* Construct a list giving information about this stack frame. */
    frame = list_new(5);
    d = list_empty_spaces(frame, 5);

    /* First element is the error code. */
    d->type = T_ERROR;
    d->u.error = ident_dup(error);
    d++;

    /* Second through fifth elements are the current method info. */
    fill_in_method_info(d);

    /* Add the frame to the list. */
    frame_data.type = LIST;
    frame_data.u.list = frame;
    traceback = list_add(traceback, &frame_data);
    list_discard(frame);

    return traceback;
}

void pop_error_action_specifier(void)
{ 
    Error_action_specifier *old;

    /* Pop the first error action specifier off that stack. */
    old = cur_frame->specifiers;
    cur_frame->specifiers = old->next;
    efree(old);
}

void pop_handler_info(void)
{
    Handler_info *old;

    /* Free the data in the first handler info specifier, and pop it off that
     * stack. */
    old = cur_frame->handler_info;
    list_discard(old->traceback);
    ident_discard(old->error);
    cur_frame->handler_info = old->next;
    efree(old);
}

INTERNAL void fill_in_method_info(cData *d)
{
    Ident method_name;

    /* The method name, or 0 for eval. */
    method_name = cur_frame->method->name;
    if (method_name == NOT_AN_IDENT) {
        d->type = INTEGER;
        d->u.val = 0;
    } else {
        d->type = SYMBOL;
        d->u.val = ident_dup(method_name);
    }
    d++;

    /* The current object. */
    d->type = OBJNUM;
    d->u.objnum = cur_frame->object->objnum;
    d++;

    /* The defining object. */
    d->type = OBJNUM;
    d->u.objnum = cur_frame->method->object->objnum;
    d++;

    /* The line number. */
    d->type = INTEGER;
    d->u.val = line_number(cur_frame->method, cur_frame->pc);
}

void bind_opcode(Int opcode, cObjnum objnum) {
    op_table[opcode].binding = objnum;
}

/* ------------------------------------------------------ */
#ifdef DRIVER_DEBUG
void init_debug(void) {     
    debug.type = INTEGER;
    debug.u.val = 0;
}   

void clear_debug(void) {   
    data_discard(&debug);
    init_debug();
}     
          
void start_debug(void) {         
    data_discard(&debug);
    debug.type = INTEGER;
    debug.u.val=1;
}   
              
void start_full_debug(void) {         
    data_discard(&debug);
    debug.type = INTEGER;
    debug.u.val=2;
}   

void get_debug(cData *d) { 
    data_dup(d, &debug);
}
#endif
