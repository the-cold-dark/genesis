/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_dbpack_h
#define cdc_dbpack_h

cBuf * pack_object (cBuf * buf, Obj * obj);
cBuf * pack_data   (cBuf * buf, cData * data);
cBuf * write_ident (cBuf * buf, Long id);
cBuf * write_long  (cBuf * buf, Long n);

void unpack_object (cBuf * buf, Long * buf_pos, Obj * obj);
void unpack_data   (cBuf * buf, Long * buf_pos, cData * data);
Long read_ident    (cBuf * buf, Long * buf_pos);
Long read_long     (cBuf * buf, Long * buf_pos);

Int  size_object(Obj * obj);
Int  size_data(cData *data);
Long size_ident(Long id);
Int  size_long(Long n);

#endif

