/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_cache_h
#define cdc_cache_h

#define CACHE_LOG_SYNC		0x0001
#define CACHE_LOG_OVERFLOW	0x0002
#define CACHE_LOG_CLEAN		0x0004

void init_cache(void);
Obj *cache_get_holder(Long objnum);
Obj *cache_retrieve(Long objnum);
Obj *cache_grab(Obj *object);
void cache_discard(Obj *obj);
Int cache_check(Long objnum);
void cache_sync(void);
Obj *cache_first(void);
Obj *cache_next(void);
void cache_sanity_check(void);
#ifdef CLEAN_CACHE
void cache_cleanup(void);
#endif
cList * cache_info(int level);

#endif

