#ifndef _web_h_
#define _web_h_

#include "modules.h"

void init_web(int argc, char ** argv);
void uninit_web(void);

#ifndef _modules_
module_t web_module = {init_web, uninit_web};
#else
extern module_t web_module;
#endif

#endif
