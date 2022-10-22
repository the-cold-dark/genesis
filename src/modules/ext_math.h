
#ifndef _math_mod_h_
#define _math_mod_h_

#include "defs.h"
#include "cdc_pcode.h"

#ifndef _ext_math_
extern module_t ext_math_module;
#endif


extern NATIVE_METHOD(minor);
extern NATIVE_METHOD(major);
extern NATIVE_METHOD(add);
extern NATIVE_METHOD(sub);
extern NATIVE_METHOD(dot);
extern NATIVE_METHOD(distance);
extern NATIVE_METHOD(cross);
extern NATIVE_METHOD(scale);
extern NATIVE_METHOD(is_lower);
extern NATIVE_METHOD(transpose);

#endif

