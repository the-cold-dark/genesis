#ifndef _veil_h_
#define _veil_h_

#include "defs.h"
#include "native.h"
#include "ident.h"

Ident pabort_id, pclose_id, popen_id;

void init_veil(int argc, char ** argv);
void uninit_veil(void);

#ifndef _veil_
extern module_t veil_module;
#endif

extern NATIVE_METHOD(buf_to_veil_pkts);
extern NATIVE_METHOD(buf_from_veil_pkts);

#endif
