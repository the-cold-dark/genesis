/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/list.h
// ---
// Declarations for ColdC lists.
*/

/* The header file ordering breaks down here; we need to run this file
 *  after data.h has completed, not just after the typedefs are done.
 *  Thus the ugly conditionals. */

#ifndef _list_h_
#define _list_h_

#include "cdc_types.h"

list_t * list_new(int len);
list_t * list_dup(list_t * list);
int      list_length(list_t * list);
data_t * list_first(list_t * list);
data_t * list_next(list_t * list, data_t * d);
data_t * list_last(list_t * list);
data_t * list_prev(list_t * list, data_t * d);
data_t * list_elem(list_t * list, int i);
data_t * list_empty_spaces(list_t * list, int spaces);
int      list_search(list_t * list, data_t * data);
int      list_cmp(list_t * l1, list_t * l2);
list_t * list_insert(list_t * list, int pos, data_t * elem);
list_t * list_add(list_t * list, data_t * elem);
list_t * list_replace(list_t * list, int pos, data_t * elem);
list_t * list_delete(list_t * list, int pos);
list_t * list_delete_element(list_t * list, data_t * elem);
list_t * list_append(list_t * list1, list_t * list2);
list_t * list_reverse(list_t * list);
list_t * list_setadd(list_t * list, data_t * elem);
list_t * list_setremove(list_t * list, data_t * elem);
list_t * list_union(list_t * list1, list_t * list2);
list_t * list_sublist(list_t * list, int start, int len);
void     list_discard(list_t * list);

#endif

