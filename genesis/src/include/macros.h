/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_macros_h
#define cdc_macros_h

#include "defs.h"
#include "native.h"
#include "util.h"

/*
// -----------------------------------------------------------------------
// define how we return first
// -----------------------------------------------------------------------
*/
#ifdef NATIVE_MODULE
# define RETURN_TRUE  return 1
# define RETURN_FALSE return 0
#else
# define RETURN_TRUE  return
# define RETURN_FALSE return
#endif

/*
// -----------------------------------------------------------------------
// common defines in both functions and natives
// -----------------------------------------------------------------------
*/

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

#ifdef NATIVE_MODULE
#define ARG_COUNT    stack_pos - arg_start
#define DEF_argc     Int argc = ARG_COUNT
#define DEF_args     cData * args = &stack[arg_start]

/* this macro is mainly handy when you want to parse the args yourself */
#define INIT_ARGC(_argc_, _expected_, _str_) \
        if (_argc_ != _expected_) \
            THROW_NUM_ERROR(_argc_, _str_)

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

#define INIT_0_OR_1_ARGS(_type_) \
        DEF_args; \
        DEF_argc; \
        CHECK_BINDING \
        if (argc) \
            INIT_ARG1(_type_)

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

#define INIT_3_ARGS(_type1_, _type2_, _type3_) \
        DEF_args; \
        CHECK_BINDING \
        INIT_ARGC(ARG_COUNT, 3, "three") \
        INIT_ARG1(_type1_) \
        INIT_ARG2(_type2_) \
        INIT_ARG3(_type3_)

#define INIT_3_OR_4_ARGS(_type1_, _type2_, _type3_, _type4_) \
        DEF_args; \
        DEF_argc; \
        CHECK_BINDING \
        switch (argc) { \
            case 4:    INIT_ARG4(_type4_) \
            case 3:    INIT_ARG3(_type3_) \
                       INIT_ARG2(_type2_) \
                       INIT_ARG1(_type1_) \
                       break; \
            default:   THROW_NUM_ERROR(argc, "three or four") \
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
#else
/*
// -----------------------------------------------------------------------
*/
#define DEF_argc     Int argc
#define DEF_args     cData * args

#define INIT_FUNC(_name_, _args_) \
        if (!CAT(func_init_, _name_) _args_) return

#define INIT_NO_ARGS() \
        if (!func_init_0()) return

#define INIT_0_OR_1_ARGS(_type1_) \
        DEF_args; DEF_argc; \
        INIT_FUNC(0_or_1, (&args, &argc, _type1_))

#define INIT_1_ARG(_type1_) \
        DEF_args; \
        INIT_FUNC(1, (&args, _type1_))

#define INIT_1_OR_2_ARGS(_type1_, _type2_) \
        DEF_args; DEF_argc; \
        INIT_FUNC(1_or_2, (&args, &argc, _type1_, _type2_))

#define INIT_2_ARGS(_type1_, _type2_) \
        DEF_args; \
        INIT_FUNC(2, (&args, _type1_, _type2_))

#define INIT_2_OR_3_ARGS(_type1_, _type2_, _type3_) \
        DEF_args; DEF_argc; \
        INIT_FUNC(2_or_3, (&args, &argc, _type1_, _type2_, _type3_))

#endif

/*
// -----------------------------------------------------------------------
*/
#define COLDC_FUNC(_name_) void CAT(func_, _name_) (void)
#define NATIVE_METHOD(_name_) \
        Int CAT(native_, _name_) (Int stack_start, Int arg_start)
#define ANY_TYPE 0

/*
// -----------------------------------------------------------------------
// native-specific defines
// -----------------------------------------------------------------------
*/
#ifdef NATIVE_MODULE

#define VARIABLE 1
#define FIXED    0

#include "execute.h"

#define CLEAN_STACK() pop_native_stack(stack_start)

#define RETURN_INTEGER(d) native_push_int(d);    RETURN_TRUE
#define RETURN_FLOAT(d)   native_push_float(d);  RETURN_TRUE
#define RETURN_OBJNUM(d)  native_push_objnum(d); RETURN_TRUE
#define RETURN_SYMBOL(d)  native_push_symbol(d); RETURN_TRUE
#define RETURN_ERROR(d)   native_push_error(d);  RETURN_TRUE
#define RETURN_STRING(d)  native_push_string(d); RETURN_TRUE
#define RETURN_BUFFER(d)  native_push_buffer(d); RETURN_TRUE
#define RETURN_FROB(d)    native_push_frob(d);   RETURN_TRUE
#define RETURN_DICT(d)    native_push_dict(d);   RETURN_TRUE
#define RETURN_LIST(d)    native_push_list(d);   RETURN_TRUE

#define CLEAN_RETURN_INTEGER(d) CLEAN_STACK(); RETURN_INTEGER(d)
#define CLEAN_RETURN_FLOAT(d)   CLEAN_STACK(); RETURN_FLOAT(d)
#define CLEAN_RETURN_OBJNUM(d)  CLEAN_STACK(); RETURN_OBJNUM(d)
#define CLEAN_RETURN_SYMBOL(d)  CLEAN_STACK(); RETURN_SYMBOL(d)
#define CLEAN_RETURN_ERROR(d)   CLEAN_STACK(); RETURN_ERROR(d)
#define CLEAN_RETURN_STRING(d)  CLEAN_STACK(); RETURN_STRING(d)
#define CLEAN_RETURN_BUFFER(d)  CLEAN_STACK(); RETURN_BUFFER(d)
#define CLEAN_RETURN_FROB(d)    CLEAN_STACK(); RETURN_FROB(d)
#define CLEAN_RETURN_DICT(d)    CLEAN_STACK(); RETURN_DICT(d)
#define CLEAN_RETURN_LIST(d)    CLEAN_STACK(); RETURN_LIST(d)

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

#define INT1           args[0].u.val
#define INT2           args[1].u.val
#define INT3           args[2].u.val
#define INT4           args[3].u.val
#define INT5           args[4].u.val
#define FLOAT1         args[0].u.fval
#define FLOAT2         args[1].u.fval
#define FLOAT3         args[2].u.fval
#define OBJNUM1        args[0].u.objnum
#define OBJNUM2        args[1].u.objnum
#define OBJNUM3        args[2].u.objnum
#define SYM1           args[0].u.symbol
#define SYM2           args[1].u.symbol
#define SYM3           args[2].u.symbol
#define ERR1           args[0].u.error
#define ERR2           args[1].u.error
#define ERR3           args[2].u.error
#define STR1           args[0].u.str
#define STR2           args[1].u.str
#define STR3           args[2].u.str
#define STR4           args[3].u.str
#define LIST1          args[0].u.list
#define LIST2          args[1].u.list
#define LIST3          args[2].u.list
#define BUF1           args[0].u.buffer
#define BUF2           args[1].u.buffer
#define BUF3           args[2].u.buffer
#define FROB1          args[0].u.frob
#define FROB2          args[1].u.frob
#define FROB3          args[2].u.frob
#define DICT1          args[0].u.dict
#define DICT2          args[1].u.dict
#define DICT3          args[2].u.dict

/*
// -----------------------------------------------------------------------
// class macros
// -----------------------------------------------------------------------
*/

#define INSTANCE_PROTOTYPES(_class_) \
    void CAT(pack_,_class_) (cData*, FILE*); \
    void CAT(unpack_,_class_) (cData*, FILE*); \
    int CAT(size_,_class_) (cData*); \
    int CAT(compare_,_class_) (cData*, cData*); \
    int CAT(hash_,_class_) (cData*); \
    void CAT(dup_,_class_) (cData*, cData*); \
    void CAT(discard_,_class_) (cData*); \
    cStr* CAT(string_,_class_) (cStr*, cData*, int)

#define INSTANCE_INIT(_class_,_name_) \
    { \
       _name_,		      \
       0,                     \
       CAT(pack_,_class_),      \
       CAT(unpack_,_class_),    \
       CAT(size_,_class_),      \
       CAT(compare_,_class_),   \
       CAT(hash_,_class_),      \
       CAT(dup_,_class_),	      \
       CAT(discard_,_class_),   \
       CAT(string_,_class_)     \
    }

#define INSTANCE_RECORD(_d_, _var_) \
     cInstance *_var_; \
     if ((_d_) < FIRST_INSTANCE || (_d_) >= LAST_INSTANCE) { \
         panic("Invalid data type"); \
     } \
     _var_ = class_registry + (_d_) - FIRST_INSTANCE
