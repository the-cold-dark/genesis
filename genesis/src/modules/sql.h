#ifndef _sql_h_
#define _sql_h_

#include "defs.h"
#include "cdc_pcode.h"

#ifndef _sql_
extern module_t sql_module;
#else
module_t sql_module = {NULL, NULL};
#endif

extern NATIVE_METHOD(dbquote_explode);

#endif
