#include "defs.h"
#include "core.h"
#include "cdc_types.h"

void init_core(int argc, char ** argv) {
    pabort_id = ident_get("abort");
    pclose_id = ident_get("close");
    popen_id  = ident_get("open");
}

void uninit_core(void) {
    ident_discard(pabort_id);
    ident_discard(pclose_id);
    ident_discard(popen_id);
}

