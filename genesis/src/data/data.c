/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <ctype.h>
#include "util.h"
#include "cache.h"
#include "token.h"
#include "lookup.h"

/* Effects: Returns 0 if and only if d1 and d2 are equal according to ColdC
 *	    conventions.  If d1 and d2 are of the same type and are integers or
 *	    strings, returns greater than 0 if d1 is greater than d2 according
 *	    to ColdC conventions, and less than 0 if d1 is less than d2. */
Int data_cmp(cData *d1, cData *d2) {
    if (d1->type == FLOAT && d2->type == INTEGER) {
        d2->type = FLOAT;
        d2->u.fval = (float) d2->u.val;
    } else if (d1->type == INTEGER && d2->type == FLOAT) {
        d1->type = FLOAT;
        d1->u.fval = (float) d1->u.val;
    }

    if (d1->type != d2->type) {
	return 1;
    }

    switch (d1->type) {

      case INTEGER:
	return d1->u.val - d2->u.val;

      case FLOAT: {
        float t=d1->u.fval - d2->u.fval;
        return (t>0 ? 1 : (t==0 ? 0 : -1));
      }

      case STRING:
	return strccmp(string_chars(d1->u.str), string_chars(d2->u.str));

      case OBJNUM:
	return (d1->u.objnum != d2->u.objnum);

      case LIST:
	return list_cmp(d1->u.list, d2->u.list);

      case SYMBOL:
	return (d1->u.symbol != d2->u.symbol);

      case T_ERROR:
	return (d1->u.error != d2->u.error);

      case FROB:
	if (d1->u.frob->cclass != d2->u.frob->cclass)
	    return 1;
	return data_cmp(&d1->u.frob->rep, &d2->u.frob->rep);

      case DICT:
	return dict_cmp(d1->u.dict, d2->u.dict);

      case BUFFER:
	if (d1->u.buffer == d2->u.buffer)
	    return 0;
	if (d1->u.buffer->len != d2->u.buffer->len)
	    return 1;
	return MEMCMP(d1->u.buffer->s, d2->u.buffer->s, d1->u.buffer->len);

      default:
	return 1;
    }
}

/* Effects: Returns 1 if data is true according to ColdC conventions, or 0 if
 *	    data is false. */
Int data_true(cData *d)
{
    switch (d->type) {

      case INTEGER:
	return (d->u.val != 0);

      case FLOAT:
        return (d->u.fval != 0.0);

      case STRING:
	return (string_length(d->u.str) != 0);

      case OBJNUM:
	return 1;

      case LIST:
	return (list_length(d->u.list) != 0);

      case SYMBOL:
	return 1;

      case T_ERROR:
	return 0;

      case FROB:
	return 1;

      case DICT:
	return (d->u.dict->keys->len != 0);

      case BUFFER:
	return (d->u.buffer->len != 0);

      default:
	return 0;
    }
}

uLong data_hash(cData *d)
{
    cList *values;

    switch (d->type) {

      case INTEGER:
	return d->u.val;

      case FLOAT:
        return *((uLong*)(&d->u.fval));

      case STRING:
	return hash_case(string_chars(d->u.str), string_length(d->u.str));

      case OBJNUM:
	return d->u.objnum;

      case LIST:
	if (list_length(d->u.list) > 0)
	    return data_hash(list_first(d->u.list));
	else
	    return 100;

      case SYMBOL:
	return hash(ident_name(d->u.symbol));

      case T_ERROR:
	return hash(ident_name(d->u.error));

      case FROB:
	return d->u.frob->cclass + data_hash(&d->u.frob->rep);

      case DICT:
	values = d->u.dict->values;
	if (list_length(values) > 0)
	    return data_hash(list_first(values));
	else
	    return 200;

      case BUFFER:
	if (d->u.buffer->len)
	    return d->u.buffer->s[0] + d->u.buffer->s[d->u.buffer->len - 1];
	else
	    return 300;

      default:
	panic("data_hash() called with invalid type");
	return -1;
    }
}

/* Modifies: dest.
 * Effects: Copies src into dest, updating reference counts as necessary. */
void data_dup(cData *dest, cData *src)
{
    dest->type = src->type;
    switch (src->type) {

      case INTEGER:
	dest->u.val = src->u.val;
	break;

      case FLOAT:
        dest->u.fval = src->u.fval;
        break;

      case STRING:
	dest->u.str = string_dup(src->u.str);
	break;

      case OBJNUM:
	dest->u.objnum = src->u.objnum;
	break;

      case LIST:
	dest->u.list = list_dup(src->u.list);
	break;

      case SYMBOL:
	dest->u.symbol = ident_dup(src->u.symbol);
	break;

      case T_ERROR:
	dest->u.error = ident_dup(src->u.error);
	break;

      case FROB:
	dest->u.frob = TMALLOC(cFrob, 1);
	dest->u.frob->cclass = src->u.frob->cclass;
	data_dup(&dest->u.frob->rep, &src->u.frob->rep);
	break;

      case DICT:
	dest->u.dict = dict_dup(src->u.dict);
	break;

      case BUFFER:
	dest->u.buffer = buffer_dup(src->u.buffer);
	break;
    }
}

/* Modifies: The value referred to by data.
 * Effects: Updates the reference counts for the value referred to by data
 *	    when we are no longer using it. */
void data_discard(cData *data)
{
    switch (data->type) {

      case STRING:
	string_discard(data->u.str);
	break;

      case LIST:
	list_discard(data->u.list);
	break;

      case SYMBOL:
	ident_discard(data->u.symbol);
	break;

      case T_ERROR:
	ident_discard(data->u.error);
	break;

      case FROB:
	data_discard(&data->u.frob->rep);
	TFREE(data->u.frob, 1);
	break;

      case DICT:
	dict_discard(data->u.dict);
	break;

      case BUFFER:
	buffer_discard(data->u.buffer);
    }
}

cStr *data_tostr(cData *data) {
    char *s;
    Number_buf nbuf;

    switch (data->type) {

      case INTEGER:
	s = long_to_ascii(data->u.val, nbuf);
	return string_from_chars(s, strlen(s));

      case FLOAT:
        s = float_to_ascii(data->u.fval,nbuf);
        return string_from_chars(s, strlen(s));

      case STRING:
	return string_dup(data->u.str);

      case OBJNUM: {
          char       prefix[] = {'$', (char) NULL};
          Obj * obj = cache_retrieve(data->u.objnum);

          if (!obj || obj->objname == -1) {
              s = long_to_ascii(data->u.objnum, nbuf);
              prefix[0] = '#';
          } else {
              s = ident_name(obj->objname);
          }

          cache_discard(obj);

          return string_add_chars(string_from_chars(prefix, 1), s, strlen(s));
      }

      case LIST:
	return string_from_chars("[list]", 6);

      case SYMBOL:
	s = ident_name(data->u.symbol);
	return string_from_chars(s, strlen(s));

      case T_ERROR:
	s = ident_name(data->u.error);
	return string_from_chars(s, strlen(s));

      case FROB:
	return string_from_chars("<frob>", 6);

      case DICT:
	return string_from_chars("#[dict]", 7);

      case BUFFER:
	return string_from_chars("`[buffer]", 9);

      default:
	panic("Unrecognized data type.");
	return NULL;
    }
}

/* Effects: Returns a string containing a printed representation of data. */
cStr *data_to_literal(cData *data, Bool objnames) {
    cStr *str = string_new(0);

    return data_add_literal_to_str(str, data, objnames);
}

cStr *data_add_list_literal_to_str(cStr *str, cList *list, Bool objnames) {
    cData *d, *next;

    str = string_addc(str, '[');
    d = list_first(list);
    if (d) {
	next = list_next(list, d);
	while (next) {
	    str = data_add_literal_to_str(str, d, objnames);
	    str = string_add_chars(str, ", ", 2);
	    d = next;
	    next = list_next(list, d);
	}
	str = data_add_literal_to_str(str, d, objnames);
    }
    return string_addc(str, ']');
}

/* Modifies: str (mutator, claims reference count).
 * Effects: Returns a string with the printed representation of data added to
 *	    it. */
cStr *data_add_literal_to_str(cStr *str, cData *data, Bool objnames) {
    char *s;
    Number_buf nbuf;
    Int i;

    switch(data->type) {

      case INTEGER:
	s = long_to_ascii(data->u.val, nbuf);
	return string_add_chars(str, s, strlen(s));

      case FLOAT:
        s=float_to_ascii(data->u.fval,nbuf);
        return string_add_chars(str, s, strlen(s));

      case STRING:
	s = string_chars(data->u.str);
	return string_add_unparsed(str, s, string_length(data->u.str));

      case OBJNUM: {
          char   pre = '$';
          Obj  * obj;

          if (objnames) {
              obj = cache_retrieve(data->u.objnum);

              if (!obj || obj->objname == -1) {
                  s = long_to_ascii(data->u.objnum, nbuf);
                  pre = '#';
              } else {
                  s = ident_name(obj->objname);
              }

              cache_discard(obj);
          } else {
              pre = '#';
              s = long_to_ascii(data->u.objnum, nbuf);
          }

	  str = string_addc(str, pre);
	  return string_add_chars(str, s, strlen(s));
      }

      case LIST:
	return data_add_list_literal_to_str(str, data->u.list, objnames);

      case SYMBOL:
	str = string_addc(str, '\'');
	s = ident_name(data->u.symbol);
	if (*s && is_valid_ident(s))
	    return string_add_chars(str, s, strlen(s));
	else
	    return string_add_unparsed(str, s, strlen(s));

      case T_ERROR:
	str = string_addc(str, '~');
	s = ident_name(data->u.error);
	if (is_valid_ident(s))
	    return string_add_chars(str, s, strlen(s));
	else
	    return string_add_unparsed(str, s, strlen(s));

      case FROB: {
        cData d;

	str = string_addc(str, '<');
        d.type = OBJNUM;
        d.u.objnum = data->u.frob->cclass;
        str = data_add_literal_to_str(str, &d, objnames);
	str = string_add_chars(str, ", ", 2);
	str = data_add_literal_to_str(str, &data->u.frob->rep, objnames);
	return string_addc(str, '>');
      }

      case DICT:
	return dict_add_literal_to_str(str, data->u.dict, objnames);

      case BUFFER:
	str = string_add_chars(str, "`[", 2);
	for (i = 0; i < data->u.buffer->len; i++) {
	    s = long_to_ascii(data->u.buffer->s[i], nbuf);
	    str = string_add_chars(str, s, strlen(s));
	    if (i < data->u.buffer->len - 1)
		str = string_add_chars(str, ", ", 2);
	}
	return string_addc(str, ']');

      default:
	return str;
    }
}

/* Effects: Returns an id (without updating reference count) for the name of
 *	    the type given by type. */
Long data_type_id(Int type)
{
    switch (type) {
      case INTEGER:	return integer_id;
      case FLOAT:	return float_id;
      case STRING:	return string_id;
      case OBJNUM:	return objnum_id;
      case LIST:	return list_id;
      case SYMBOL:	return symbol_id;
      case T_ERROR:	return error_id;
      case FROB:	return frob_id;
      case DICT:	return dictionary_id;
      case BUFFER:	return buffer_id;
      default:		panic("Unrecognized data type."); return 0;
    }
}

