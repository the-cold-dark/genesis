
#ifndef _HANDLED_FROB_H_
#define _HANDLED_FROB_H_

typedef struct {
    cObjnum cclass;
    cData rep;
    Ident handler;
} HandledFrob;

#define HANDLED_FROB(_d_) ((HandledFrob*)((_d_)->u.instance))

#endif

