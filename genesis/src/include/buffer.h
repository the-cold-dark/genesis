/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/buffer.h
// ---
// Declarations for ColdC buffers.
*/

#ifndef _buffer_h_
#define _buffer_h_

#include "cdc_types.h"

buffer_t * buffer_new(int len);
buffer_t * buffer_dup(buffer_t *buf);
void       buffer_discard(buffer_t *buf);
buffer_t * buffer_append(buffer_t *buf1, buffer_t *buf2);
int        buffer_retrieve(buffer_t *buf, int pos);
buffer_t * buffer_replace(buffer_t *buf, int pos, unsigned int c);
buffer_t * buffer_add(buffer_t *buf, unsigned int c);
buffer_t * buffer_resize(buffer_t *buf, int len);
buffer_t * buffer_tail(buffer_t *buf, int pos);
string_t * buffer_to_string(buffer_t *buf);
buffer_t * buffer_from_string(string_t * string);
list_t   * buffer_to_strings(buffer_t *buf, buffer_t *sep);
buffer_t * buffer_from_strings(list_t *string_list, buffer_t *sep);
buffer_t * buffer_subrange(buffer_t *buf, int start, int len);
buffer_t * buffer_prep(buffer_t *buf);

#define buffer_len(__b) (__b->len)

#endif

