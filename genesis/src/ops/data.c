/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include "cdc_pcode.h"
#include "cdc_db.h"
#include "handled_frob.h"

COLDC_FUNC(size) {
    Int size;

    INIT_0_OR_1_ARGS(ANY_TYPE);

    if (argc) {
        size = size_data(&args[0]);
        pop(1);
    } else {
        size = size_object(cur_frame->object);
    }

    push_int(size);
}

COLDC_FUNC(type) {
    Int type;

    INIT_1_ARG(ANY_TYPE);

    type = args[0].type;
    pop(1);
    push_symbol(data_type_id(type));
}

COLDC_FUNC(frob_class) {
    Long cclass;
    Int type;

    INIT_1_ARG(ANY_TYPE);

    type = args[0].type;
    if (type == FROB)
	cclass = args[0].u.frob->cclass;
    else if (type == HANDLED_FROB_TYPE)
	cclass = HANDLED_FROB(&args[0])->cclass;
    else {
	cthrow(type_id,
	       "The argument (%D) is not a frob.",
	       &args[0]);
	return;
    }
    pop(1);
    push_objnum(cclass);
}

COLDC_FUNC(frob_value) {
    Int type;
    cData d;

    INIT_1_ARG(ANY_TYPE);

    type = args[0].type;
    if (type == FROB)
	data_dup(&d, &args[0].u.frob->rep);
    else if (type == HANDLED_FROB_TYPE)
	data_dup(&d, &HANDLED_FROB(&args[0])->rep);
    else {
	cthrow(type_id,
	       "The argument (%D) is not a frob.",
	       &args[0]);
	return;
    }
    data_discard(&args[0]);
    args[0]=d;
}

COLDC_FUNC(frob_handler) {
    Int type;
    Ident handler;

    INIT_1_ARG(ANY_TYPE);

    type = args[0].type;
    if (type == (int)HANDLED_FROB_TYPE)
	handler = ident_dup(HANDLED_FROB(&args[0])->handler);
    else {
	cthrow(type_id,
	       "The argument (%D) is not a frob with method handler.",
	       &args[0]);
	return;
    }
    pop(1);
    push_symbol(handler);
    ident_discard(handler);
}

COLDC_FUNC(toint) {
    Long val = 0;

    INIT_1_ARG(ANY_TYPE);

    switch (args[0].type) {
        case STRING:
            val = atol(string_chars(args[0].u.str)); break;
        case FLOAT:
            val = (Long) args[0].u.fval; break;
        case OBJNUM:
            val = args[0].u.objnum; break;
        case INTEGER:
            return;
        default:
            cthrow(type_id,
                 "The first argument (%D) is not a number, objnum or string.",
                   &args[0]);
            return;
    }

    pop(1);
    push_int(val);
}

COLDC_FUNC(tofloat) {
      Float val = 0;
  
      INIT_1_ARG(ANY_TYPE);
  
      switch (args[0].type) {
          case STRING:
              val = atof(string_chars(args[0].u.str)); break;
          case INTEGER:
              val = (Float) args[0].u.val; break;
          case FLOAT:
              return;
          case OBJNUM:
              val = (Float) args[0].u.objnum; break;
          default:
              cthrow(type_id,
                "The first argument (%D) is not a number, objnum or string.",
                &args[0]);
              return;
      }
      pop(1);
      push_float(val);
}

COLDC_FUNC(tostr) {
    cStr *str;

    INIT_1_ARG(ANY_TYPE);

    str = data_tostr(&args[0]);

    pop(1);
    push_string(str);
    string_discard(str);
}

COLDC_FUNC(toliteral) {
    cStr *str;

    INIT_1_ARG(ANY_TYPE);

    str = data_to_literal(&args[0], TRUE);

    pop(1);
    push_string(str);
    string_discard(str);
}

COLDC_FUNC(fromliteral) {
    cData d;

    INIT_1_ARG(STRING);

    data_from_literal(&d, string_chars(STR1));

    if (d.type == -1)
        THROW((type_id, "Unable to parse data \"%s\"", string_chars(STR1)))

    string_discard(STR1);
    args[0] = d;
}

COLDC_FUNC(toobjnum) {
    INIT_1_ARG(INTEGER);

    if (INT1 < 0)
        cthrow(type_id, "Objnums must be 0 or greater");

    args[0].u.objnum = args[0].u.val;
    args[0].type = OBJNUM;
}

COLDC_FUNC(tosym) {
    Long sym;

    INIT_1_ARG(STRING);

    /* no NULL symbols */
    if (string_length(STR1) < 1)
        THROW((symbol_id, "Symbols must be one or more characters."))

    /* this is wrong, we should check this everywhere, not just here,
       but at the moment everywhere assumes 'ident_get' returns a valid
       ident irregardless */
    if (!string_is_valid_ident(STR1))
        THROW((symbol_id, "Symbol contains non-alphanumeric characters."))

    sym = ident_get_string(STR1);

    pop(1);
    push_symbol(sym);
}

COLDC_FUNC(toerr) {
    Ident error;

    INIT_1_ARG(SYMBOL);

    error = SYM1;
    args[0].type = T_ERROR;
    args[0].u.error = error;
}

COLDC_FUNC(valid) {
    Int is_valid;

    INIT_1_ARG(ANY_TYPE);

    if (args[0].type == OBJNUM)
        is_valid = VALID_OBJECT(OBJNUM1);
    else
        is_valid = NO;

    pop(1);
    push_int(is_valid);
}

