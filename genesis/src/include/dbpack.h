/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_dbpack_h
#define cdc_dbpack_h

cBuf * pack_object (cBuf * buf, Obj * obj);
cBuf * pack_data   (cBuf * buf, cData * data);
cBuf * write_ident (cBuf * buf, Ident id);
cBuf * write_long  (cBuf * buf, Long n);
cBuf * write_float (cBuf * buf, Float f);

void  unpack_object (cBuf * buf, Long * buf_pos, Obj * obj);
void  unpack_data   (cBuf * buf, Long * buf_pos, cData * data);
Ident read_ident    (cBuf * buf, Long * buf_pos);
Long  read_long     (cBuf * buf, Long * buf_pos);
Float read_float    (cBuf * buf, Long * buf_pos);

Int  size_object(Obj * obj);
Int  size_data(cData *data);
Int  size_ident(Ident id);
Int  size_long(Long n);
Int  size_float(Float f);

#endif

