/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Floating point functions (including trig) originally coded by
// Andy Selle (andy@positronic.res.cmu.edu)
//
// converted to natives by Brandon Gillespie, a bit of optimization performed
// on how args are handled.  Standardized floats to float hooks, ColdC ints
// to double hooks in the math libs.  Added a few more hooks.
*/

#include "defs.h"

#include <time.h>
#include <math.h>
#include "cdc_pcode.h"
#include "util.h"
#include "sig.h"
#ifdef __svr4__
#include <ieeefp.h>
#endif

#define RESET_FPE (caught_fpe = 0)

#define HANDLE_FPE \
        if (caught_fpe) { \
            RESET_FPE; \
            THROW((fpe_id, "floating-point exception")); \
        }

#ifdef HAVE_FINITE
#define CHECK_FINITE(__x) \
        if (!finite((double) __x)) \
            THROW((inf_id, "Infinite result."))
#else
#define CHECK_FINITE(__x)
#endif

/* man: no */
COLDC_FUNC(sin) {
    cData * args;
    double  r;

    if (!func_init_1(&args, FLOAT))
        return;

    r = sin((double) FLOAT1);

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: ? */
COLDC_FUNC(exp) {
    cData  * args;
    double   r;

    if (!func_init_1(&args, FLOAT))
        return;

    r = exp((double) FLOAT1);

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: ? */
COLDC_FUNC(log) {
    double   r;
    cData  * args;

    if (!func_init_1(&args, FLOAT))
        return;

    RESET_FPE;

    r = log((double) FLOAT1);

    HANDLE_FPE;

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: no */
COLDC_FUNC(cos) {
    cData  * args;
    double   r;

    if (!func_init_1(&args, FLOAT))
        return;

    r = cos((double) FLOAT1);

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: no? */
COLDC_FUNC(tan) {
    double   r;
    cData  * args;

    if (!func_init_1(&args, FLOAT))
        return;

    RESET_FPE;

    r = tan((double) FLOAT1);

    HANDLE_FPE;

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: ? */
COLDC_FUNC(sqrt) {
    double   r;
    cData  * args;

    if (!func_init_1(&args, FLOAT))
        return;

    RESET_FPE;

    r = sqrt((double) FLOAT1);

    HANDLE_FPE;

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: ? */
COLDC_FUNC(asin) {
    double   r;
    cData  * args;

    if (!func_init_1(&args, FLOAT))
        return;

    RESET_FPE;

    r = asin((double) FLOAT1);

    HANDLE_FPE;

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: ? */
COLDC_FUNC(acos) {
    double   r;
    cData  * args;

    if (!func_init_1(&args, FLOAT))
        return;

    RESET_FPE;

    r = acos((double) FLOAT1);

    HANDLE_FPE;

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: ? */
COLDC_FUNC(atan) {
    cData  * args;
    double   r;

    if (!func_init_1(&args, FLOAT))
        return;

    r = atan((double) FLOAT1);

    CHECK_FINITE(r);

    pop(1);
    push_float((cFloat) r);
}

/* man: yes */
COLDC_FUNC(pow) {
    double   r;
    cData  * args;

    if (!func_init_2(&args, FLOAT, FLOAT))
        return;

    RESET_FPE;

    r = pow((double) FLOAT1, (double) FLOAT2);

    HANDLE_FPE;

    CHECK_FINITE(r);

    pop(2);

    push_float((cFloat) r);
}

COLDC_FUNC(atan2) {
    double   r;
    cData  * args;

    if (!func_init_2(&args, FLOAT, FLOAT))
        return;

    RESET_FPE;

    r = atan2((double) FLOAT1, (double) FLOAT2);

    HANDLE_FPE;

    CHECK_FINITE(r);

    pop(2);

    push_float((cFloat) r);
}

#ifndef HAVE_RINT
/* not the best replacement, but it works -- Brandon */
double rint (double num) {
    double whole = floor(num);

    if ((num - whole) >= 0.5)
        return ceil(num);
    else
        return whole;
}
#endif

COLDC_FUNC(round) {
    double   r;
    cData  * args;

    if (!func_init_1(&args, FLOAT))
        return;

    r = rint((double) FLOAT1);

    pop(1);

    push_int((cNum) r);
}

COLDC_FUNC(random) {
    cData * args;

    /* Take one integer argument. */
    if (!func_init_1(&args, INTEGER))
        return;

    /* If INT1 is negative, throw ~range */
    if (INT1 <= 0) {
        cthrow(range_id, "Maximum value was less than 0.");
        return;  
    }
 
    /* Replace argument on stack with a random number. */
    INT1 = random_number(INT1) + 1;
}

/* which is 1 for max, -1 for min. */
INTERNAL void find_extreme(Int which) {
    Int arg_start, num_args, i, type;
    cData *args, *extreme, d;

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

COLDC_FUNC(max) {
    find_extreme(1);
}

COLDC_FUNC(min) {
    find_extreme(-1);
}

COLDC_FUNC(abs) {
    cData * args;

    if (!func_init_1(&args, ANY_TYPE))
        return;

    if (args[0].type == INTEGER) {
        if (INT1 < 0)
            INT1 = -INT1;
    } else if (args[0].type == FLOAT) {
        FLOAT1 = (cFloat) fabs((double) FLOAT1);
    } else {
        cthrow(type_id, "Argument (%D) is not an integer or float.", &args[ARG1]);
        return;
    }
}

