/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/object.h
// ---
*/

#ifndef _macros_h_
#define _macros_h_

#include "defs.h"    /* CAT() comes from defs.h */
#include "native.h"
#include "util.h"

/*
// -----------------------------------------------------------------------
// define how we return first
// -----------------------------------------------------------------------
*/
#ifdef NATIVE_MODULE
  #define RETURN_TRUE  return 1
  #define RETURN_FALSE return 0
#else
  #define RETURN_TRUE  return
  #define RETURN_FALSE return
#endif

/*
// -----------------------------------------------------------------------
// common defines in both functions and natives
// -----------------------------------------------------------------------
*/
/* this doesn't vary from functions to native methods */
#define ARG_COUNT    stack_pos - arg_start
#define DEF_argc     int argc = ARG_COUNT
#define DEF_args     data_t * args = &stack[arg_start]

#define THROW_NUM_ERROR(_num_, _str_) { \
        func_num_error(_num_, _str_); \
        RETURN_FALSE; \
    }

#define THROW_TYPE_ERROR(_type_, _name_, _pos_) { \
        func_type_error(_name_, &stack[arg_start+_pos_],english_type(_type_)); \
        RETURN_FALSE; \
    }

#define THROW(_args_) { \
        cthrow _args_ ; \
        RETURN_FALSE; \
    }

#ifdef NATIVE_MODULE
#define CHECK_BINDING
#else
#define INVALID_BINDING \
    (op_table[cur_frame->last_opcode].binding != INV_OBJNUM && \
     op_table[cur_frame->last_opcode].binding != \
     cur_frame->method->object->objnum)
#define FUNC_NAME()    (op_table[cur_frame->last_opcode].name)
#define FUNC_BINDING() (op_table[cur_frame->last_opcode].binding)

#define CHECK_BINDING \
    if (INVALID_BINDING) \
        THROW((perm_id, "%s() is bound to %O", FUNC_NAME(), FUNC_BINDING()));
#endif

/* this macro is mainly handy when you want to parse the args yourself */
#define INIT_ARGC(_argc_, _expected_, _str_) \
        if (_argc_ != _expected_) \
            THROW_NUM_ERROR(_argc_, _str_);

#define CHECK_TYPE(_pos_, _type_, _str_) \
        if (args[_pos_].type != _type_) \
            THROW_TYPE_ERROR(_type_, _str_, _pos_)

#define CHECK_OPT_TYPE(_pos_, _type_, _str_) \
        if (argc > _pos_ && args[_pos_].type != _type_) \
            THROW_TYPE_ERROR(_type_, _str_, _pos_)

/* arg specific tests */
#define INIT_ARG1(_type_)      CHECK_TYPE(0, _type_, "first")
#define INIT_OPT_ARG1(_type_)  CHECK_OPT_TYPE(0, _type_, "first")
#define INIT_ARG2(_type_)      CHECK_TYPE(1, _type_, "second")
#define INIT_OPT_ARG2(_type_)  CHECK_OPT_TYPE(1, _type_, "second")
#define INIT_ARG3(_type_)      CHECK_TYPE(2, _type_, "third")
#define INIT_OPT_ARG3(_type_)  CHECK_OPT_TYPE(2, _type_, "third")
#define INIT_ARG4(_type_)      CHECK_TYPE(3, _type_, "fourth")
#define INIT_OPT_ARG4(_type_)  CHECK_OPT_TYPE(3, _type_, "fourth")

#define INIT_NO_ARGS() \
        CHECK_BINDING \
        INIT_ARGC(ARG_COUNT, 0, "none")

#define INIT_1_ARG(_type_) \
        DEF_args; \
        CHECK_BINDING \
        INIT_ARGC(ARG_COUNT, 1, "one") \
        INIT_ARG1(_type_) \

#define INIT_0_OR_1_ARG(_type_) \
        DEF_args; \
        DEF_argc; \
        CHECK_BINDING \
        INIT_ARG1(_type_) \
        else \
            THROW_NUM_ERROR(argc, "zero or one")

#define INIT_2_ARGS(_type1_, _type2_) \
        DEF_args; \
        CHECK_BINDING \
        INIT_ARGC(ARG_COUNT, 2, "two") \
        INIT_ARG1(_type1_) \
        INIT_ARG2(_type2_)

#define INIT_1_OR_2_ARGS(_type1_, _type2_) \
        DEF_args; \
        DEF_argc; \
        CHECK_BINDING \
        switch (argc) { \
            case 2:   INIT_ARG2(_type2_) \
            case 1:   INIT_ARG1(_type1_) \
                      break; \
            default:  THROW_NUM_ERROR(argc, "one or two") \
        }

#define INIT_3_ARGS(_type1_, _type2_, _type3_) \
        DEF_args; \
        CHECK_BINDING \
        INIT_ARGC(ARG_COUNT, 3, "three") \
        INIT_ARG1(_type1_) \
        INIT_ARG2(_type2_) \
        INIT_ARG3(_type3_)

#define INIT_2_OR_3_ARGS(_type1_, _type2_, _type3_) \
        DEF_args; \
        DEF_argc; \
        CHECK_BINDING \
        switch (argc) { \
            case 3:    INIT_ARG3(_type3_) \
            case 2:    INIT_ARG2(_type2_) \
                       INIT_ARG1(_type1_) \
                       break; \
            default:   THROW_NUM_ERROR(argc, "two or three") \
        }

#define INIT_1_TO_3_ARGS(_type1_, _type2_, _type3_) \
        DEF_args; \
        DEF_argc; \
        CHECK_BINDING \
        switch (argc) { \
            case 3:    INIT_ARG3(_type3_) \
            case 2:    INIT_ARG2(_type2_) \
            case 1:    INIT_ARG1(_type1_) \
                       break; \
            default:   THROW_NUM_ERROR(argc, "one to three") \
        }

#define COLDC_FUNC(_name_) void CAT(func_, _name_) (void)
#define NATIVE_METHOD(_name_) \
        int CAT(native_, _name_) (int arg_start, data_t * rval)

/*
// -----------------------------------------------------------------------
// native-specific defines
// -----------------------------------------------------------------------
*/
#ifdef NATIVE_MODULE

#define VARIABLE 1
#define FIXED    0
#define ANY_DATA 0

#define RETURN_INTEGER(d){rval->type = INTEGER; rval->u.val = d; RETURN_TRUE;}
#define RETURN_FLOAT(d)  {rval->type = FLOAT; rval->u.fval = d; RETURN_TRUE;}
#define RETURN_OBJNUM(d) {rval->type = OBJNUM; rval->u.objnum = d; RETURN_TRUE;}
#define RETURN_SYMBOL(d) {rval->type = SYMBOL; rval->u.symbol = d; RETURN_TRUE;}
#define RETURN_ERROR(d)  {rval->type = ERROR; rval->u.error = d; RETURN_TRUE;}
#define RETURN_STRING(d) {rval->type = STRING; rval->u.str = d; RETURN_TRUE;}
#define RETURN_BUFFER(d) {rval->type = BUFFER; rval->u.buffer = d; RETURN_TRUE;}
#define RETURN_FROB(d)   {rval->type = FROB; rval->u.frob = d; RETURN_TRUE;}
#define RETURN_DICT(d)   {rval->type = DICT; rval->u.dict = d; RETURN_TRUE;}
#define RETURN_LIST(d)   {rval->type = LIST; rval->u.list = d; RETURN_TRUE;}

#else /* NATIVE_MODULE */
/*
// -----------------------------------------------------------------------
// function-specific defines
// -----------------------------------------------------------------------
*/

/* This will have problems with standard C types */
#define HOOK_1_ARG(_name_, _hook_, _in_type_, _out_type_) \
    FUNC_NAME(_name_) { \
        CAT(_out_type_, _t) var; \
        INIT_1_ARG(_in_type_) \
        var = _hook_ (args[0].u._in_type_); \
        pop(1); \
        CAT(push_, _in_type_) (var); \
    }


#endif /* NATIVE_MODULE */

#endif

/*
// -----------------------------------------------------------------------
// arg and data macros, mainly for familiarity to ColdC
// -----------------------------------------------------------------------
*/

#define _ARG(_p_) args[_p_]
#define ARG1 0
#define ARG2 1
#define ARG3 2
#define ARG4 3
#define ARG5 4
#define ARG6 5

#define TYPE(_pos_)     args[_pos_].type

#define _INT(_pos_)    args[_pos_].u.val
#define _FLOAT(_pos_)  args[_pos_].u.fval
#define _OBJNUM(_pos_) args[_pos_].u.objnum
#define _SYM(_pos_)    args[_pos_].u.symbol
#define _ERR(_pos_)    args[_pos_].u.error
#define _STR(_pos_)    args[_pos_].u.str
#define _LIST(_pos_)   args[_pos_].u.list
#define _BUF(_pos_)    args[_pos_].u.buffer
#define _FROB(_pos_)   args[_pos_].u.frob
#define _DICT(_pos_)   args[_pos_].u.dict

