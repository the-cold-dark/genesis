#ifndef _veil_h_
#define _veil_h_

#include "defs.h"
#include "cdc_pcode.h"

#ifdef VEIL_C
Ident pabort_id, pclose_id, popen_id;
#else
extern Ident pabort_id, pclose_id, popen_id;
#endif

void init_veil(Int argc, char ** argv);
void uninit_veil(void);

#ifndef _veil_
extern module_t veil_module;
#endif

extern NATIVE_METHOD(buf_to_veil_pkts);
extern NATIVE_METHOD(buf_from_veil_pkts);

#endif
