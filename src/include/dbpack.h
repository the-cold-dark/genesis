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

void  unpack_object (const cBuf * buf, Long * buf_pos, Obj * obj);
void  unpack_data   (const cBuf * buf, Long * buf_pos, cData * data);
Ident read_ident    (const cBuf * buf, Long * buf_pos);
Long  read_long     (const cBuf * buf, Long * buf_pos);
Float read_float    (const cBuf * buf, Long * buf_pos);

Int  size_object(Obj * obj, int memory_size);
Int  size_data(cData *data, int memory_size);
Int  size_ident(Ident id, int memory_size);
Int  size_long(Long n, int memory_size);
Int  size_float(Float f, int memory_size);

#endif

