/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <time.h>
#include <string.h>
#ifdef __UNIX__
#include <sys/time.h>    /* for mtime() */
#endif
#ifdef __MSVC__
#include <windows.h>
#endif

#include "operators.h"
#include "execute.h"
#include "util.h"
#include "opcodes.h"


#ifndef HAVE_TM_GMTOFF
static int last_gmtoffcheck = -1;
static int gmt_offset = -1;
#endif

void func_anticipate_assignment(void) {
    Int opcode, ind;
    Long id;
    cData *dp, d;
    Frame *caller_frame = cur_frame->caller_frame;
    Int pc;

    if (!func_init_0())
	return;
    if (!caller_frame) {
	push_int(1);
	return;
    }

    pc=caller_frame->pc;

    /* Most of this is from anticipate_assignment() */
    
    /* skip error handling */
    while ((opcode = caller_frame->opcodes[pc]) == CRITICAL_END)
	pc++;

    switch (opcode) {
      case SET_LOCAL:
        /* Zero out local variable value. */
        dp = &stack[caller_frame->var_start +
                    caller_frame->opcodes[pc + 1]];
        data_discard(dp);
        dp->type = INTEGER;
        dp->u.val = 0;
        break;
      case SET_OBJ_VAR:
        /* Zero out the object variable, if it exists. */
        ind = caller_frame->opcodes[pc + 1];
        id = object_get_ident(caller_frame->method->object, ind);
        d.type = INTEGER;
        d.u.val = 0;
        object_assign_var(caller_frame->object, caller_frame->method->object,
                          id, &d);
        break;
    }
    push_int(1);
    return;
}

void func_time(void) {
    /* Take no arguments. */
    if (!func_init_0())
	return;

    push_int(time(NULL));
}

void func_localtime(void) {
    struct tm * tms;
    cData     * d;
    cList     * l;
    time_t      t;
    cData     * args;
    Int         nargs;
#ifndef HAVE_TM_GMTOFF
    struct tm * gtms;
#endif

    if (!func_init_0_or_1(&args, &nargs, INTEGER))
	return;

    if (nargs) {
        t = (time_t) args[0].u.val;
        pop(1);
    } else
        time(&t);

#ifdef __BORLANDC__
    if (t < 18000) {
        THROW((type_id,
     "Borland's time util is broken, and requires time values above 18000"))
    }
#endif

    tms = localtime(&t);

    l = list_new(12);
    d = list_empty_spaces(l, 12);

    /* Add one to certain elements to make them 1-x instead of 0-x */
    d[0].type=INTEGER;
    d[0].u.val = (cNum) t;
    d[1].type=INTEGER;
    d[1].u.val = tms->tm_sec;
    d[2].type=INTEGER;
    d[2].u.val = tms->tm_min;
    d[3].type=INTEGER;
    d[3].u.val = tms->tm_hour;
    d[4].type=INTEGER;
    d[4].u.val = tms->tm_mday;
    d[5].type=INTEGER;
    d[5].u.val = tms->tm_mon + 1;
    d[6].type=INTEGER;
    d[6].u.val = tms->tm_year;
    d[7].type=INTEGER;
    d[7].u.val = tms->tm_wday + 1;
    d[8].type=INTEGER;
    d[8].u.val = tms->tm_yday + 1;
    d[9].type=INTEGER;
    d[9].u.val = tms->tm_isdst;
    d[10].type = STRING;
    d[10].u.str= string_dup(str_tzname);
    d[11].type=INTEGER;
#ifdef HAVE_TM_GMTOFF
    d[11].u.val = tms->tm_gmtoff;
#else
    if (last_gmtoffcheck != tms->tm_yday) {
        int hour = tms->tm_hour; /* they use the same internal structure */
        gtms = gmtime(&t);
        gmt_offset = ((hour - gtms->tm_hour) * 60 * 60);
        last_gmtoffcheck = tms->tm_yday;
    }
    d[11].u.val = gmt_offset;
#endif

    push_list(l);
    list_discard(l);
}

#ifdef __Win32__
void func_mtime(void) {
    LARGE_INTEGER freq, cnt;

    if (!func_init_0())
        return;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    push_int((cNum) (((cnt.QuadPart * 1000000) / freq.QuadPart) % 1000000));
}
#else
#ifdef HAVE_GETTIMEOFDAY
void func_mtime(void) {
    struct timeval tp;

    if (!func_init_0())
        return;

    /* usec is microseconds */
    gettimeofday(&tp, NULL);

    push_int((cNum) tp.tv_usec);
}
#else
void func_mtime(void) {
    if (!func_init_0())
        return;

    push_int(-1);
}
#endif
#endif

void func_ctime(void) {
    cData *args;
    Int num_args;
    time_t tval;
    char *timestr;
    cStr *str;

    /* Take an optional integer argument. */
    if (!func_init_0_or_1(&args, &num_args, INTEGER))
	return;

    tval = (num_args) ? args[0].u.val : time(NULL);

#ifdef __BORLANDC__
    if (tval < 18000) {
        pop(num_args);
        THROW((type_id,
     "Borland's time util is broken, and requires time values above 18000"))
    }
#endif

    timestr = ctime(&tval);
    str = string_from_chars(timestr, 24);

    pop(num_args);
    push_string(str);
    string_discard(str);
}

void func_bind_function(void) {
    cData * args;
    Int      opcode;

    /* accept a symbol and objnum */
    if (!func_init_2(&args, SYMBOL, OBJNUM))
        return;

    opcode = find_function(ident_name(args[0].u.symbol));

    if (opcode == -1) {
        cthrow(perm_id, "Attempt to bind function which does not exist.");
        return;
    }

    op_table[opcode].binding = args[1].u.objnum;

    pop(2);
    push_int(1);
}

void func_unbind_function(void) {
    cData *args;
    Int   opcode;

    /* accept a symbol */
    if (!func_init_1(&args, SYMBOL))
        return;

    opcode = find_function(ident_name(args[0].u.symbol));

    if (opcode == -1) {
        cthrow(perm_id, "Attempt to unbind function which does not exist.");
        return;
    }

    op_table[opcode].binding = INV_OBJNUM;

    pop(1);
    push_int(1);
}

#ifdef DRIVER_DEBUG
void func_debug_callers(void) {
    cData *args;

    if (!func_init_1(&args, INTEGER))
        return;

    if (args[0].u.val == 0)
        clear_debug();
    else if (args[0].u.val == 2)
        start_full_debug();
    else
        start_debug();
}

void func_call_trace(void) {
    if (!func_init_0())
      return;

    check_stack(1);
    get_debug(&stack[stack_pos++]);
}
#endif
