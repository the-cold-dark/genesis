/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_cache_h
#define cdc_cache_h

#ifndef _cache_
extern Int cache_width;
extern Int cache_depth;
#else
Int cache_width = 10;
Int cache_depth = 30;
#endif

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

#endif

