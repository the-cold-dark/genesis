#define NATIVE_MODULE "$math"

#include <math.h>
#include "ext_math.h"
#include "list.h"

module_t ext_math_module = {NO, NULL, NO, NULL};

static int check_one_vector(cList *l1, Int *len_ret)
{
    Int i,len;

    len=list_length(l1);
    for (i=0; i<len; i++) {
	if (list_elem(l1,i)->type != FLOAT)
	    THROW((type_id, "Arguments must be lists of floats."))
    }
    *len_ret=len;
    RETURN_TRUE;
}

static int check_vectors(cList *l1, cList *l2, Int *len_ret)
{
    Int i,len;

    len=list_length(l1);
    if (list_length(l2)!=len)
	THROW((range_id, "Arguments are not of the same length."))
    for (i=0; i<len; i++) {
	if (list_elem(l1,i)->type != FLOAT)
	    THROW((type_id, "Arguments must be lists of floats."))
	if (list_elem(l2,i)->type != FLOAT)
	    THROW((type_id, "Arguments must be lists of floats."))
    }
    *len_ret=len;
    RETURN_TRUE;
}


NATIVE_METHOD(minor) {
    Int i,len;
    cList *l,*l1,*l2;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
	RETURN_FALSE;
    l=list_new(len);
    l->len=len;
    for (i=0; i<len; i++) {
	Float p,q;

	p=list_elem(l1,i)->u.fval;
	q=list_elem(l2,i)->u.fval;
	list_elem(l,i)->type=FLOAT;
	list_elem(l,i)->u.fval=p<q ? p : q;
    }
    CLEAN_RETURN_LIST(l);
}

NATIVE_METHOD(major) {
    Int i,len;
    cList *l,*l1,*l2;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
	RETURN_FALSE;
    l=list_new(len);
    l->len=len;
    for (i=0; i<len; i++) {
	Float p,q;

	p=list_elem(l1,i)->u.fval;
	q=list_elem(l2,i)->u.fval;
	list_elem(l,i)->type=FLOAT;
	list_elem(l,i)->u.fval=p>q ? p : q;
    }
    CLEAN_RETURN_LIST(l);
}

NATIVE_METHOD(add) {
    Int i,len;
    cList *l,*l1,*l2;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
	RETURN_FALSE;
    l=list_new(len);
    l->len=len;
    for (i=0; i<len; i++) {
	Float p,q;

	p=list_elem(l1,i)->u.fval;
	q=list_elem(l2,i)->u.fval;
	list_elem(l,i)->type=FLOAT;
	list_elem(l,i)->u.fval=p+q;
    }
    CLEAN_RETURN_LIST(l);
}

NATIVE_METHOD(sub) {
    Int i,len;
    cList *l,*l1,*l2;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
	RETURN_FALSE;
    l=list_new(len);
    l->len=len;
    for (i=0; i<len; i++) {
	Float p,q;

	p=list_elem(l1,i)->u.fval;
	q=list_elem(l2,i)->u.fval;
	list_elem(l,i)->type=FLOAT;
	list_elem(l,i)->u.fval=p-q;
    }
    CLEAN_RETURN_LIST(l);
}

NATIVE_METHOD(dot) {
    Int i,len;
    cList *l1,*l2;
    Float s;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
	RETURN_FALSE;
    for (s=0.0,i=0; i<len; i++) {
	Float p,q;

	p=list_elem(l1,i)->u.fval;
	q=list_elem(l2,i)->u.fval;
	s+=p*q;
    }
    CLEAN_RETURN_FLOAT(s);
}

NATIVE_METHOD(distance) {
    Int i,len;
    cList *l1,*l2;
    Float s;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
	RETURN_FALSE;
    for (s=0.0,i=0; i<len; i++) {
	Float p,q,d;

	p=list_elem(l1,i)->u.fval;
	q=list_elem(l2,i)->u.fval;
	d=p-q;
	s+=d*d;
    }
    CLEAN_RETURN_FLOAT(sqrt(s));
}

NATIVE_METHOD(cross) {
    Int len;
    cList *l,*l1,*l2;
    cData *f,*f1,*f2;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
	RETURN_FALSE;
    if (len!=3)
	THROW((range_id,"The vectors are not of length 3."))
    l=list_new(len);
    l->len=len;
    f=list_elem(l,0);
    f1=list_elem(l1,0);
    f2=list_elem(l2,0);
    f[0].type=f[1].type=f[2].type=FLOAT;
    f[0].u.fval=f1[1].u.fval*f2[2].u.fval-f1[2].u.fval*f2[1].u.fval;
    f[1].u.fval=f1[2].u.fval*f2[0].u.fval-f1[0].u.fval*f2[2].u.fval;
    f[2].u.fval=f1[0].u.fval*f2[1].u.fval-f1[1].u.fval*f2[0].u.fval;
    CLEAN_RETURN_LIST(l);
}

NATIVE_METHOD(scale) {
    Int i,len;
    cList *l,*l1;
    Float f;

    INIT_2_ARGS(FLOAT,LIST);
    l1=LIST2;
    f=FLOAT1;
    if (!check_one_vector (l1,&len))
	RETURN_FALSE;
    l=list_new(len);
    l->len=len;
    for (i=0; i<len; i++) {
	Float p;

	p=list_elem(l1,i)->u.fval;
	list_elem(l,i)->type=FLOAT;
	list_elem(l,i)->u.fval=p*f;
    }
    CLEAN_RETURN_LIST(l);
}

NATIVE_METHOD(is_lower) {
    Int i,len;
    cList *l1,*l2;

    INIT_2_ARGS(LIST,LIST);
    l1=LIST1;
    l2=LIST2;
    if (!check_vectors (l1,l2,&len))
        RETURN_FALSE;
    for (i=0; i<len; i++) {
        Float p,q;

        p=list_elem(l1,i)->u.fval;
        q=list_elem(l2,i)->u.fval;
        if (p>=q) {
            CLEAN_RETURN_INTEGER(0);
        }
    }
    CLEAN_RETURN_INTEGER(1);
}

NATIVE_METHOD(transpose) {
    Int i,len,len1;
    cList *l,*l1;
    cData *e,*o;

    INIT_1_ARG(LIST);
    l1=LIST1;
    len=list_length(l1);
    if (!len) {
        l1=list_dup(l1);
        CLEAN_RETURN_LIST(l1);
    }
    e=list_elem(l1,0);
    for (i=0; i<len; i++) {
        if (e[i].type!=LIST)
            THROW((type_id,"The argument must be a list of lists."))
    }
    len1=list_length(e[0].u.list);
    if (!len1) {
        l1=list_dup(e[0].u.list);
        CLEAN_RETURN_LIST(l1);
    }
    for (i=1; i<len; i++) {
        if (list_length(e[i].u.list)!=len1)
            THROW((range_id,"All sublists must be of the same length"))
    }
    l=list_new(len1);
    l->len=len1;
    o=list_elem(l,0);
    for (i=0; i<len1; i++) {
        cList *l2;
        cData *k;
        Int j;

        l2=list_new(len);
        l2->len=len;
        o[i].type=LIST;
        o[i].u.list=l2;
        k=list_elem(l2,0);
        for (j=0; j<len; j++)
            data_dup(&k[j],list_elem(e[j].u.list,i));
    }
    CLEAN_RETURN_LIST(l);
}
