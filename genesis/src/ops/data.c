/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
*/

#include "config.h"
#include "defs.h"

#include "lookup.h"
#include "execute.h"
#include "dbpack.h"
#include "token.h"
#include "cache.h"

void func_size(void) {
    data_t * args;
    int      nargs,
             size;

    if (!func_init_0_or_1(&args, &nargs, 0))
	return;

    if (nargs) {
        size = size_data(&args[0]);
        pop(1);
    } else {
        size = size_object(cur_frame->object);
    }

    /* Push size of current object. */
    push_int(size);
}

void func_type(void) {
    data_t *args;
    int type;

    /* Accept one argument of any type. */
    if (!func_init_1(&args, 0))
	return;

    /* Replace argument with symbol for type name. */
    type = args[0].type;
    pop(1);
    push_symbol(data_type_id(type));
}

void func_class(void) {
    data_t *args;
    long cclass;

    /* Accept one argument of frob type. */
    if (!func_init_1(&args, FROB))
	return;

    /* Replace argument with class. */
    cclass = args[0].u.frob->cclass;
    pop(1);
    push_objnum(cclass);
}

void func_toint(void) {
    data_t *args;
    long val = 0;

    /* Accept a string or integer to convert into an integer. */
    if (!func_init_1(&args, 0))
	return;

    switch (args[0].type) {
        case STRING:
            val = atol(string_chars(args[0].u.str)); break;
        case FLOAT:
            val = (long) args[0].u.fval; break;
        case OBJNUM:
            val = args[0].u.objnum; break;
        case INTEGER:
            return;
        default:
            cthrow(type_id,
                   "The first argument (%D) is not an integer or string.",
                   &args[0]);
            return;
    }

    pop(1);
    push_int(val);
}

void func_tofloat(void) {
      data_t * args;
      float val = 0;
  
      /* Accept a string, integer or integer to convert into a float. */
      if (!func_init_1(&args, 0))
          return;
  
      switch (args[0].type) {
          case STRING:
              val = atof(string_chars(args[0].u.str)); break;
          case INTEGER:
              val = (float) args[0].u.val; break;
          case FLOAT:
              return;
          case OBJNUM:
              val = (float) args[0].u.objnum; break;
          default:
              cthrow(type_id,
                "The first argument (%D) is not an integer or string.",
                &args[0]);
              return;
      }
      pop(1);
      push_float(val);
}

void func_tostr(void) {
    data_t *args;
    string_t *str;

    /* Accept one argument of any type. */
    if (!func_init_1(&args, 0))
	return;

    /* Replace the argument with its text version. */
    str = data_tostr(&args[0]);
    pop(1);
    push_string(str);
    string_discard(str);
}

void func_toliteral(void) {
    data_t *args;
    string_t *str;

    /* Accept one argument of any type. */
    if (!func_init_1(&args, 0))
	return;

    /* Replace the argument with its unparsed version. */
    str = data_to_literal(&args[0]);
    pop(1);
    push_string(str);
    string_discard(str);
}

void func_toobjnum(void) {
    data_t *args;

    /* Accept an integer to convert into a objnum. */
    if (!func_init_1(&args, INTEGER))
	return;

    if (args[0].u.val < 0)
        cthrow(type_id, "Objnums must be 0 or greater");

    args[0].u.objnum = args[0].u.val;
    args[0].type = OBJNUM;
}

void func_tosym(void) {
    data_t *args;
    long sym;

    /* Accept one string argument. */
    if (!func_init_1(&args, STRING))
	return;

    /* this is wrong, we should check this everywhere, not just here,
       but at the moment everywhere assumes 'ident_get' returns a valid
       ident irregardless */
    if (!is_valid_ident(string_chars(args[0].u.str))) {
        cthrow(type_id,
        "Symbols may only contain alphanumeric characters and the underscore.");
        return;
    }

    sym = ident_get(string_chars(args[0].u.str));
    pop(1);
    push_symbol(sym);
}

void func_toerr(void) {
    data_t *args;
    long error;

    /* Accept one string argument. */
    if (!func_init_1(&args, STRING))
	return;

    error = ident_get(string_chars(args[0].u.str));
    pop(1);
    push_error(error);
}

void func_valid(void) {
    data_t *args;
    int is_valid;

    /* Accept one argument of any type (only objnums can be valid, though). */
    if (!func_init_1(&args, 0))
	return;

    is_valid = (args[0].type == OBJNUM && cache_check(args[0].u.objnum));
    pop(1);
    push_int(is_valid);
}

