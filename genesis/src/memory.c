/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Memory management.
//
// This code is not ANSI-conformant, because it plays some games with pointer
// conversions which ANSI does not allow.  It also assumes that a Long has the
// most restrictive alignment.  I do the pile and tray mallocs this way because
// they work on most systems and save space.
*/

#include "defs.h"

#include <sys/types.h>

/* This file supports a tray malloc and a pile malloc.  The tray malloc
 * enhances allocation efficiency by keeping trays for small pieces of
 * data.  Note that tfree() and trealloc() require the size of the old
 * block.  trealloc() handles ptr == NULL and newsize == 0 appropriately.
 *
 * The pile malloc enhances efficiency and convenience for applications
 * that allocate a lot of memory in small chunks and then free it all at
 * once.  We call new_pile() to get a handle on a 'pile' that we can
 * allocate memory from.  pile_malloc() accepts a pile in addition to the
 * size argument.  pile_free() frees all the used memory in a pile.  It
 * retains up to MAX_BLOCKS blocks of memory in the pile to avoid repeated
 * mallocs and frees of large blocks. */

#define MAX(a, b)	(((a) >= (b)) ? (a) : (b))

#define TRAY_INC	sizeof(Tlist)
#define NUM_TRAYS	4
#define MAX_USE_TRAY	(NUM_TRAYS * TRAY_INC)
#define TRAY_ELEM	508

#define PILE_BLOCK_SIZE 254
#define MAX_PILE_BLOCKS 8

typedef struct tlist Tlist;

struct tlist {
    Tlist *next;
};

typedef struct blink_s blink_t;

struct blink_s {
    void    * data;
    size_t    sz;
    blink_t * next;
};

struct pile {
    blink_t *blocks;
};

static Tlist *trays[NUM_TRAYS];

static Bool inside_emalloc_logger = FALSE;

#ifdef DOFUNC_FREE
void efree(void *block) {
    free(block);
}
#endif

void * emalloc(size_t size) {
    void *ptr;

    ptr = malloc(size);
    if (!ptr)
        panic("emalloc(%lX) failed.", size);

    if (log_malloc_size &&
        (log_malloc_size <= size) &&
        !inside_emalloc_logger)
    {
        inside_emalloc_logger = TRUE;
        write_err("Allocation of size %l at:", size);
        log_current_task_stack(FALSE, write_err);
        inside_emalloc_logger = FALSE;
    }

    return ptr;
}

void *erealloc(void *ptr, size_t size)
{
    void *newptr;

    newptr = realloc(ptr, size);
    if (!newptr)
	panic("erealloc(%ld) failed.", size);

    if (log_malloc_size &&
        (log_malloc_size <= size) &&
        !inside_emalloc_logger)
    {
        inside_emalloc_logger = TRUE;
        write_err("Allocation of size %l at:", size);
        log_current_task_stack(FALSE, write_err);
        inside_emalloc_logger = FALSE;
    }

    return newptr;
}

void *tmalloc(size_t size)
{
    Int t, n, i;
    void *p;

    /* If the block isn't fairly small, fall back on malloc(). */
    if (size > MAX_USE_TRAY)
	return emalloc(size);

    /* Find the appropriate tray to use. */
    t = (size - 1) / TRAY_INC;

    /* If we're out of tray elements, make a new tray. */
    if (!trays[t]) {
	trays[t] = EMALLOC(Tlist, TRAY_ELEM);

	/* n is the number of Tlists we need for each tray element. */
	n = t + 1;

	/* Link up tray elements. */
	for (i = 0; i <= TRAY_ELEM - (2 * n); i += n)
	    trays[t][i].next = &trays[t][i + n];
	trays[t][i].next = NULL;
    }

    /* Return the first tray element, and set trays[t] to the next tray
     * element. */
    p = (void *) trays[t];
    trays[t] = trays[t]->next;
    return p;
}

void tfree(void *ptr, size_t size)
{
    Int t;

    /* If the block size is greater than MAX_USE_TRAY, then tmalloc() didn't
     * pull it out of a tray, so just free it normally. */
    if (size > MAX_USE_TRAY) {
	efree(ptr);
	return;
    }

    /* Add this element to the appropriate tray. */
    t = (size - 1) / TRAY_INC;
    ((Tlist *) ptr)->next = trays[t];
    trays[t] = (Tlist *) ptr;
}

void *trealloc(void *ptr, size_t oldsize, size_t newsize)
{
    void *cnew;

    /* If neither the old block or the new block is fairly small, then just
     * fall back on realloc(). */
    if (oldsize > MAX_USE_TRAY && newsize > MAX_USE_TRAY)
	return erealloc(ptr, newsize);

    /* If sizes are such that we would be using the same tray for both blocks,
     * just return the old pointer. */
    if ((oldsize - 1) / TRAY_INC == (newsize - 1) / TRAY_INC)
	return ptr;

    /* Allocate a new tray, copy into it, and free the old tray. */
    cnew = tmalloc(newsize);
    memcpy(cnew, ptr, MAX(newsize, oldsize));
    tfree(ptr, oldsize);

    return cnew;
}

/* Duplicate a string, using tray memory. */
char *tstrdup(char *s) {
    Int len = strlen(s);
    char *cnew;

    cnew = TMALLOC(char, len + 1);
    if (cnew)
      memcpy(cnew, s, len + 1);
    else
      panic("tstrdup(): malloc(%ld) failed.", len);

    return cnew;
}

char *tstrndup(char *s, Int len) {
    char *cnew;

    cnew = TMALLOC(char, len + 1);
    memcpy(cnew, s, len);
    cnew[len] = (char) NULL;
    return cnew;
}

/* Frees a tray-allocated string, assuming we
   allocated exactly enough memory for it. */
void tfree_chars(char *s) {
  if (s)
    tfree(s, strlen(s) + 1);
}

Pile *new_pile(void) {
    Pile *tmp;
    /* static Int pile_counter=0; */

    tmp=emalloc(sizeof(Pile));
    tmp->blocks=NULL;

    return tmp;
}

void free_pile(Pile *tmp) {
    pfree(tmp);
    efree(tmp);
}

void * pmalloc(Pile *p, size_t s) {
    blink_t * blink;

    blink = (blink_t*)emalloc(sizeof(blink_t));
    blink->data = tmalloc(s);
    blink->sz = s;
    blink->next = (blink_t *) p->blocks;
    p->blocks = blink;
    return blink->data;
}

void pfree(Pile *p) {
      blink_t *roam,*ahead;

      roam = p->blocks;
      while (roam) {
          ahead = roam->next;
          tfree(roam->data, roam->sz);
          efree(roam);
          roam = ahead;
      }
      p->blocks = NULL;
}
