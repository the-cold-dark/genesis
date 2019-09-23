/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"
#include "functions.h"
#include "execute.h"

/* ----------------------------------------------------------------- */
/* cancel a suspended task                                           */
COLDC_FUNC(task_info) {
    cList * list;
    cData *args;

    if (!func_init_1(&args, INTEGER))
        return;

    list = vm_info(INT1);

    if (!list)
        THROW((type_id, "No task %d.", INT1));
    pop(1);
    push_list(list);
    list_discard(list);
}

/* ----------------------------------------------------------------- */
/* cancel a suspended task                                           */
COLDC_FUNC(cancel) {
    cData *args;

    if (!func_init_1(&args, INTEGER))
        return;


    if (!vm_lookup(args[0].u.val)) {
        cthrow(type_id, "No task %d.", args[0].u.val);
    } else {
        vm_cancel(args[0].u.val);
        pop(1);
        push_int(1);
    }
}

/* ----------------------------------------------------------------- */
/* suspend a task                                                    */
COLDC_FUNC(suspend) {

    if (!func_init_0())
        return;

    if (atomic) {
        cthrow(atomic_id, "Attempt to suspend while executing atomically.");
        return;
    }

    vm_suspend();

    /* we'll let task_resume push something onto the stack for us */
}

/* ----------------------------------------------------------------- */
COLDC_FUNC(resume) {
    cData *args;
    Int nargs;
    Long tid;

    if (!func_init_1_or_2(&args, &nargs, INTEGER, 0))
        return;

    tid = args[0].u.val;

    if (!vm_lookup(tid)) {
        cthrow(type_id, "No task %d.", args[0].u.val);
    } else {
        if (nargs == 1)
            vm_resume(tid, NULL);
        else
            vm_resume(tid, &args[1]);
        pop(nargs);
        push_int(1);
    }
}

/* ----------------------------------------------------------------- */
COLDC_FUNC(pause) {
    if (!func_init_0())
        return;

    push_int(1);

    if (atomic) {
        if (cur_frame->ticks <= REFRESH_METHOD_THRESHOLD)
            cur_frame->ticks = PAUSED_METHOD_TICKS;
    } else {
        vm_pause();
    }
}

/* ----------------------------------------------------------------- */
COLDC_FUNC(atomic) {
    cData * args;

    if (!func_init_1(&args, INTEGER))
        return;

    if (!coldcc)
        atomic = (Bool) (args[0].u.val ? true : false);

    pop(1);
    push_int(atomic ? 1 : 0);
}

/* ----------------------------------------------------------------- */
COLDC_FUNC(refresh) {

    if (!func_init_0())
        return;

    push_int(1);

    if (cur_frame->ticks <= REFRESH_METHOD_THRESHOLD) {
        if (atomic) {
            cur_frame->ticks = PAUSED_METHOD_TICKS;
        } else {
            vm_pause();
        }
    }
}

/* ----------------------------------------------------------------- */
COLDC_FUNC(tasks) {
    cList * list;

    if (!func_init_0())
        return;

    list = vm_list();

    push_list(list);
    list_discard(list);
}

/* ----------------------------------------------------------------- */
COLDC_FUNC(tick) {
    if (!func_init_0())
        return;
    push_int(tick);
}

/* ----------------------------------------------------------------- */
COLDC_FUNC(stack) {
    VMState * vm = NULL;
    Frame   * frame = NULL;
    cData   * args;
    Int       nargs;
    Bool      want_lineno = true;

    if (!func_init_0_to_2(&args, &nargs, INTEGER, INTEGER))
        return;

    if ((nargs == 0) || (INT1 == task_id)) {
        frame = cur_frame;
    } else {
        vm = vm_lookup(INT1);
        if (vm)
            frame = vm->cur_frame;
    }

    if ((nargs == 2) && (args[1].u.val == 0))
        want_lineno = false;

    if (frame) {
        cList * list;

        list = vm_stack(cur_frame, want_lineno);

        pop(nargs);
        push_list(list);
        list_discard(list);
    } else {
        cthrow(type_id, "No task %d.", args[0].u.val);
    }
}

COLDC_FUNC(calling_method) {
    /* Accept no arguments, and push the name of the calling method */
    if (!func_init_0())
        return;

    if (cur_frame->caller_frame)
        push_symbol(cur_frame->caller_frame->method->name);
    else
        push_int(0);
}

COLDC_FUNC(method) {
    if (!func_init_0())
        return;

    push_symbol(cur_frame->method->name);
}

COLDC_FUNC(this) {
    /* Accept no arguments, and push the objnum of the current object. */
    if (!func_init_0())
        return;
    push_objnum(cur_frame->object->objnum);
}

COLDC_FUNC(definer) {
    /* Accept no arguments, and push the objnum of the method definer. */
    if (!func_init_0())
        return;
    push_objnum(cur_frame->method->object->objnum);
}

COLDC_FUNC(sender) {
    /* Accept no arguments, and push the objnum of the sending object. */
    if (!func_init_0())
        return;
    if (cur_frame->sender == NOT_AN_IDENT)
        push_int(0);
    else
        push_objnum(cur_frame->sender);
}

COLDC_FUNC(caller) {
    /* Accept no arguments, and push the objnum of the calling method's
     * definer. */
    if (!func_init_0())
        return;
    if (cur_frame->caller == NOT_AN_IDENT)
        push_int(0);
    else
        push_objnum(cur_frame->caller);
}

COLDC_FUNC(task_id) {
    /* Accept no arguments, and push the task ID. */
    if (!func_init_0())
        return;
    push_int(task_id);
}

COLDC_FUNC(ticks_left) {
    if (!func_init_0())
      return;

    push_int(cur_frame->ticks);
}

COLDC_FUNC(user) {
    if (!func_init_0())
        return;
    if (cur_frame->user == NOT_AN_IDENT)
        push_int(0);
    else
        push_objnum(cur_frame->user);
}

COLDC_FUNC(set_user) {
    if (!func_init_0())
        return;
    cur_frame->user = cur_frame->object->objnum;
    push_objnum(cur_frame->user);
}

