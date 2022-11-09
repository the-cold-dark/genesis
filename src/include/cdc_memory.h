/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_memory_h
#define cdc_memory_h

typedef struct pile Pile;

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

void uninit_emalloc(void);
void * emalloc(size_t size);
void * erealloc(void *ptr, size_t size);
void * tmalloc(size_t size);
void   tfree(void *ptr, size_t size);
void * trealloc(void *ptr, size_t oldsize, size_t newsize);
char * tstrdup(const char *s);
char * tstrndup(const char *s, Int len);
void   tfree_chars(char *s);
Pile * new_pile(void);
void   free_pile(Pile *tmp);
void * pmalloc(Pile *pile, size_t size);
void  pfree(Pile *pile);

#define efree(what) free(what)

#define EMALLOC(type, num)         ((type *) emalloc((num) * sizeof(type)))
#define EREALLOC(ptr, type, num) ((type *) erealloc(ptr, (num) * sizeof(type)))
#define TMALLOC(type, num)         ((type *) tmalloc((num) * sizeof(type)))
#define TFREE(ptr, num)                 tfree(ptr, (num) * sizeof(*(ptr)))
#define TREALLOC(ptr, type, old, new) \
    ((type *) trealloc(ptr, (old) * sizeof(*(ptr)), (new) * sizeof(type)))
#define PMALLOC(pile, type, num) ((type *) pmalloc(pile, (num) * sizeof(type)))

#define MEMCPY(a, b, l)                memcpy(a, b, (l) * sizeof(*(a)))
#define MEMMOVE(a, b, l)        memmove(a, b, (l) * sizeof(*(a)))
#define MEMCMP(a, b, l)                memcmp(a, b, (l) * sizeof(*(a)))

#endif
