/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define NATIVE_MODULE "$integer"

#include "cdc.h"

NATIVE_METHOD(and) {
    Int val;

    INIT_2_ARGS(INTEGER, INTEGER);

    val = (Int) INT1 & INT2;

    CLEAN_RETURN_INTEGER(val);
}

NATIVE_METHOD(or) {
    Int val;

    INIT_2_ARGS(INTEGER, INTEGER);

    val = (Int) INT1 | INT2;

    CLEAN_RETURN_INTEGER(val);
}

NATIVE_METHOD(xor) {
    Int val;

    INIT_2_ARGS(INTEGER, INTEGER);

    val = (Int) INT1 ^ INT2;

    CLEAN_RETURN_INTEGER(val);
}

NATIVE_METHOD(shleft) {
    Int val;

    INIT_2_ARGS(INTEGER, INTEGER);

    val = (Int) INT1 << INT2;

    CLEAN_RETURN_INTEGER(val);
}

NATIVE_METHOD(shright) {
    Int val;

    INIT_2_ARGS(INTEGER, INTEGER);

    val = (Int) INT1 >> INT2;

    CLEAN_RETURN_INTEGER(val);
}

NATIVE_METHOD(not) {
    Int val;

    INIT_1_ARG(INTEGER);

    val = (Int) ~ INT1;

    CLEAN_RETURN_INTEGER(val);
}

