#ifndef _web_h_
#define _web_h_

#include "defs.h"
#include "cdc_pcode.h"

void init_web(Int argc, char ** argv);
void uninit_web(void);

#ifndef _web_
extern module_t web_module;
#endif

NATIVE_METHOD(decode);
NATIVE_METHOD(encode);
NATIVE_METHOD(html_escape);

#endif
