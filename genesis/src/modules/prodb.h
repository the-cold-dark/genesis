#ifndef _prodb_h_
#define _prodb_h_

#include "defs.h"
#include "cdc_pcode.h"

#ifndef _prodb_
extern module_t prodb_module;
#else
module_t prodb_module = {NULL, NULL};
#endif

extern NATIVE_METHOD(dbquote_explode);

#endif
