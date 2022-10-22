/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_types_h
#define cdc_types_h

/*
// C typedef of any Cold advanced data type begins with a 'c'
*/

typedef Float             cFloat;
typedef Long              cNum;
typedef struct cStr       cStr;
typedef struct cList      cList;
typedef struct cBuf       cBuf;
typedef struct cFrob      cFrob;
typedef struct cData      cData;
typedef struct cDict      cDict;
typedef        Long       Ident;
typedef        Long       cObjnum;
typedef struct Obj        Obj;
typedef struct Method     Method;

typedef struct ident_entry  Ident_entry;
typedef struct string_entry String_entry;
typedef struct var          Var;
typedef struct error_list   Error_list;
typedef Int                 Object_string;
typedef Int                 Object_ident;

#include "regexp.h"

struct cStr {
    Int start;
    Int len;
    Int size;
    Int refs;
    regexp * reg;
    char s[1];
};

struct cBuf {
    Int len;
    Int size;
    Int refs;
    unsigned char s[1];
};

struct cData {
    union {
        cNum       val;
        cFloat     fval;
        cObjnum    objnum;
        Ident      symbol;
        Ident      error;
        cStr     * str;
        cList    * list;
        cFrob    * frob;
        cDict    * dict;
        cBuf     * buffer;
        void     * instance;
#ifdef USE_PARENT_OBJS
        Obj         * object;
#endif
    } u;
    Int type;
};

struct cList {
    Int start;
    Int len;
    Int size;
    Int refs;
    cData el[1];
};

struct cDict {
    cList  * keys;
    cList  * values;
    Int    * links;
    Int    * hashtab;
    Int      hashtab_size;
    Int      refs;
};

struct cFrob {
    Long cclass;
    cData rep;
};


typedef cBuf * (*ciWrData)  (cBuf*, cData*);
typedef void   (*ciRdData)  (cBuf*, Long*, cData*);
typedef int    (*ciSzData)  (cData*, int);
typedef int    (*ciHashData)(cData*);
typedef int    (*ciCmpData) (cData*, cData*);
typedef void   (*ciDupData) (cData*, cData*);
typedef void   (*ciDisData) (cData*);
typedef cStr * (*ciStrData) (cStr*, cData*, int);

typedef struct cInstance {
    char      *name;
    Ident      id_name;
    ciWrData   pack;
    ciRdData   unpack;
    ciSzData   size;
    ciCmpData  compare;
    ciHashData hash;
    ciDupData  dup;
    ciDisData  discard;
    ciStrData  addstr;
} cInstance;

/* io.h shouldn't necessarily be here, but it has to be here before other
   data types, but after the above data definitions */
#include "io.h"

#include "ident.h"
#include "object.h"
#include "list.h"
#include "cdc_string.h"
#include "buffer.h"
#include "dict.h"
#include "data.h"

#endif

