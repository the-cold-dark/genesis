/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_buffer_h
#define cdc_buffer_h

cBuf  * buffer_new(Int len);
cBuf  * buffer_dup(cBuf *buf);
void    buffer_discard(cBuf *buf);
cBuf  * buffer_append(cBuf *buf1, cBuf *buf2);
Int     buffer_retrieve(cBuf *buf, Int pos);
cBuf  * buffer_replace(cBuf *buf, Int pos, uInt c);
cBuf  * buffer_add(cBuf *buf, uInt c);
cBuf  * buffer_resize(cBuf *buf, Int len);
cStr  * buf_to_string(cBuf *buf);
cBuf  * buffer_from_string(cStr * string);
cList * buf_to_strings(cBuf *buf, cBuf *sep);
cBuf  * buffer_from_strings(cList *string_list, cBuf *sep);
cBuf  * buffer_subrange(cBuf *buf, Int start, Int len);
cBuf  * buffer_prep(cBuf *buf);
int     buffer_index(cBuf * buf, uChar * ss, int slen, int origin);

#define buffer_len(__b) (__b->len)

#endif

