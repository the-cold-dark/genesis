/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_math.c
// ---
// Floating point functions (including trig)
// Module written by Andy Selle (andy@positronic.res.cmu.edu)
//
// Now its just General extended math functions
//
// converted to natives by Brandon Gillespie, a bit of optimization performed
// on how args are handled.  Standardized floats to float hooks, ColdC ints
// to double hooks in the math libs.  Added a few more hooks.
*/

#define NATIVE_MODULE "$math"

#include "config.h"
#include "defs.h"

#include <time.h>
#include <sys/time.h>    /* for mtime() */
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"
#include "util.h"
#include <math.h>

#ifndef PI
#define PI 3.141592654
#endif

#if 0
#define SET_ARG(_var_, _pos_, _type_) \
        switch(args[_pos_].type) { \
            case INTEGER: \
                _var_ = (_type_) args[_pos_].u.val; \
                break; \
            case FLOAT: \
                _var_ = (_type_) args[_pos_].u.fval; \
                break; \
            default: \
                THROW((type_id, "must be a float or an int")); \
        }

#define INIT_MATH_1(_type_) \
        DEF_args; \
        _type_ arg = 0; \
        if (ARG_COUNT != 1) \
            THROW_NUM_ERROR(ARG_COUNT, "one"); \
        SET_ARG(arg, 0, _type_)

#define INIT_MATH_2(_type_) \
        DEF_args; \
        _type_ arg1 = 0, arg2 = 0; \
        if (ARG_COUNT != 2) \
            THROW_NUM_ERROR(ARG_COUNT, "two"); \
        SET_ARG(arg, 0, _type_) \
        SET_ARG(arg, 1, _type_)

#define HANDLE_FPE(_c_hook_) \
        if (caught_fpe) { \
            caught_fpe = 0; \
            THROW((fpe_id, "floating-point exception")); \
        } \

#define FUNC_HOOK_FPE_2(_m_name_, _c_name_, _type_, _r_type_) \
    NATIVE_METHOD(_m_name_) { \
        _r_type_ r = 0; \
        INIT_MATH_2(double); \
        r = _c_name_ (arg1, arg2); \
        HANDLE_FPE; \
        RETURN_INT((_r_type_) r);\
    }

#define MATH_HOOK_FPE(_name_) \
    NATIVE_METHOD(_name_) { \
        double r = 0; \
        INIT_MATH_1(double); \
        r = _name_ (arg); \
        HANDLE_FPE; \
        RETURN_FLOAT((FLOAT_TYPE) r);\
    }

#define MATH_HOOK(_name_) \
    NATIVE_METHOD(_name_) { \
        INIT_MATH_1(double); \
        RETURN_INT((FLOAT_TYPE) _name_ (arg));\
    }

MATH_HOOK(sin)
MATH_HOOK(exp)
MATH_HOOK_FPE(log)
MATH_HOOK(cos)
MATH_HOOK_FPE(tan)
MATH_HOOK_FPE(sqrt)
MATH_HOOK_FPE(asin)
MATH_HOOK_FPE(acos)
MATH_HOOK(atan)
abs/max/min/random
FUNC_HOOK(absf, fabs, float, float)
FUNC_HOOK_FPE_2(pow, pow, double, long)
FUNC_HOOK_FPE_2(powf, powf, float, float)
FUNC_HOOK_FPE_2(atan2, atan2, double, long)
FUNC_HOOK_FPE_2(atan2, atan2, float, float)
#endif

COLDC_FUNC(random) {
    data_t * args;

    /* Take one integer argument. */
    if (!func_init_1(&args, INTEGER))
	return;

    /* Replace argument on stack with a random number. */
    _INT(ARG1) = random_number(_INT(ARG1)) + 1;
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

COLDC_FUNC(max) {
    find_extreme(1);
}

COLDC_FUNC(min) {
    find_extreme(-1);
}

COLDC_FUNC(abs) {
    data_t *args;

    if (!func_init_1(&args, INTEGER))
	return;

    if (_INT(ARG1) < 0)
	_INT(ARG1) = -_INT(ARG1);
}

