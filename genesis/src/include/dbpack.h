/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_dbpack_h
#define cdc_dbpack_h

void pack_object(Obj * obj, FILE * fp);
void unpack_object(Obj * obj, FILE * fp);
Int  size_object(Obj * obj);

void pack_data(cData *data, FILE *fp);
void unpack_data(cData *data, FILE *fp);
Int  size_data(cData *data);

Long read_ident(FILE *fp);
void write_ident(Long id, FILE *fp);
Int  size_ident(Long id);

void write_long(Long n, FILE * fp);
Long read_long(FILE * fp);
Int  size_long(Long n);

void  write_float(Float n, FILE * fp);
Float read_float(FILE * fp);
Int   size_float(Float n);

#endif

