/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
*/

#include "config.h"
#include "defs.h"

#include <time.h>
#include <string.h>
#include <sys/time.h>    /* for mtime() */

#include "operators.h"
#include "execute.h"
#include "cdc_types.h"
#include "util.h"
#include "opcodes.h"

#if defined(sys_ultrix4_4)
int gettimeofday (struct timeval *tp, struct timezone *tzp);
#endif


INTERNAL void find_extreme(int which);

void func_random(void) {
    data_t *args;

    /* Take one integer argument. */
    if (!func_init_1(&args, INTEGER))
	return;

    /* Replace argument on stack with a random number. */
    args[0].u.val = random_number(args[0].u.val) + 1;
}

/* which is 1 for max, -1 for min. */
INTERNAL void find_extreme(int which) {
    int arg_start, num_args, i, type;
    data_t *args, *extreme, d;

    arg_start = arg_starts[--arg_pos];
    args = &stack[arg_start];
    num_args = stack_pos - arg_start;

    if (!num_args) {
	cthrow(numargs_id, "Called with no arguments, requires at least one.");
	return;
    }

    type = args[0].type;
    if (type != INTEGER && type != STRING && type != FLOAT) {
	cthrow(type_id, "First argument (%D) not an integer, float or string.",
	      &args[0]);
	return;
    }

    extreme = &args[0];
    for (i = 1; i < num_args; i++) {
	if (args[i].type != type) {
	    cthrow(type_id, "Arguments are not all of same type.");
	    return;
	}
	if (data_cmp(&args[i], extreme) * which > 0)
	    extreme = &args[i];
    }

    /* Replace args[0] with extreme, and pop other arguments. */
    data_dup(&d, extreme);
    data_discard(&args[0]);
    args[0] = d;
    pop(num_args - 1);
}

void func_max(void) {
    find_extreme(1);
}

void func_min(void) {
    find_extreme(-1);
}

void func_abs(void) {
    data_t *args;

    if (!func_init_1(&args, INTEGER))
	return;

    if (args[0].u.val < 0)
	args[0].u.val = -args[0].u.val;
}

void func_time(void) {
    /* Take no arguments. */
    if (!func_init_0())
	return;

    push_int(time(NULL));
}

void func_localtime(void) {
    struct tm * tms;
    data_t * d;
    list_t * l;
    time_t t;
    register int x;

    if (!func_init_0())
	return;

    time(&t);
    tms = localtime(&t);

    l = list_new(11);
    d = list_empty_spaces(l, 11);
    for (x=0; x < 10; x++)
        d[x].type = INTEGER;

    d[0].u.val = (int) t;
    d[1].u.val = tms->tm_sec;
    d[2].u.val = tms->tm_min;
    d[3].u.val = tms->tm_hour;
    d[4].u.val = tms->tm_mday;
    d[5].u.val = tms->tm_mon;
    d[6].u.val = tms->tm_year;
    d[7].u.val = tms->tm_wday;
    d[8].u.val = tms->tm_yday;
    d[9].u.val = tms->tm_isdst;

    d[10].type = STRING;
#ifdef HAVE_TM_ZONE
    d[10].u.str = string_from_chars(tms->tm_zone, strlen(tms->tm_zone));
#else
  #ifdef HAVE_TZNAME
    d[10].u.str = string_from_chars(tzname, strlen(tzname));
  #else
    d[10].u.str = string_new(0);
  #endif
#endif

    push_list(l);
    list_discard(l);
}

void func_mtime(void) {
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tp;
#endif

    if (!func_init_0())
        return;

#ifdef HAVE_GETTIMEOFDAY
    /* usec is microseconds */
    gettimeofday(&tp, NULL);

    push_int((int) tp.tv_usec);
#else
    push_int(-1);
#endif
}

void func_ctime(void) {
    data_t *args;
    int num_args;
    time_t tval;
    char *timestr;
    string_t *str;

    /* Take an optional integer argument. */
    if (!func_init_0_or_1(&args, &num_args, INTEGER))
	return;

    tval = (num_args) ? args[0].u.val : time(NULL);
    timestr = ctime(&tval);
    str = string_from_chars(timestr, 24);

    pop(num_args);
    push_string(str);
    string_discard(str);
}

void func_bind_function(void) {
    data_t * args;
    int      opcode;

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
    data_t *args;
    int   opcode;

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
