#ifndef _core_h_
#define _core_h_

#include "defs.h"
#include "modules.h"
#include "ident.h"

Ident pabort_id, pclose_id, popen_id;

void init_core(int argc, char ** argv);
void uninit_core(void);

#ifndef _modules_
module_t core_module = {init_core, uninit_core};
#else
extern module_t core_module;
#endif

#endif
