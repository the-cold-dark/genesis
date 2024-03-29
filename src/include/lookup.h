/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_lookup_h
#define cdc_lookup_h

/* this just makes sure we don't include it twice */
#ifndef did_sys_types
#define did_sys_types
#include <sys/types.h>
#endif

void    lookup_open(const char *name, bool cnew);
void    lookup_close(void);
void    lookup_sync(void);
bool    lookup_retrieve_objnum(cObjnum objnum, off_t *offset, Int *size);
bool    lookup_store_objnum(cObjnum objnum, off_t offset, Int size);
bool    lookup_remove_objnum(cObjnum objnum);
cObjnum lookup_first_objnum(void);
cObjnum lookup_next_objnum(void);
bool    lookup_retrieve_name(Ident name, cObjnum *objnum);
bool    lookup_store_name(Ident name, cObjnum objnum);
bool    lookup_remove_name(Ident name);

#endif

