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

Buffer   * buffer_new(int len);
Buffer   * buffer_dup(Buffer *buf);
void       buffer_discard(Buffer *buf);
Buffer   * buffer_append(Buffer *buf1, Buffer *buf2);
int        buffer_retrieve(Buffer *buf, int pos);
Buffer   * buffer_replace(Buffer *buf, int pos, unsigned int c);
Buffer   * buffer_add(Buffer *buf, unsigned int c);
Buffer   * buffer_resize(Buffer *buf, int len);
Buffer   * buffer_tail(Buffer *buf, int pos);
string_t * buffer_to_string(Buffer *buf);
Buffer   * buffer_from_string(string_t * string);
list_t   * buffer_to_strings(Buffer *buf, Buffer *sep);
Buffer   * buffer_from_strings(list_t *string_list, Buffer *sep);

#define buffer_len(__b) (__b->len)
/* int      buffer_len(Buffer *buf); */

#endif

