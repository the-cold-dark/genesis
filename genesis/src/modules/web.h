#ifndef _web_h_
#define _web_h_

#include "native.h"

void init_web(int argc, char ** argv);
void uninit_web(void);

#ifndef _web_
extern module_t web_module;
#endif

NATIVE_METHOD(decode);
NATIVE_METHOD(encode);

#endif
