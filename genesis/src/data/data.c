/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <ctype.h>
#include "util.h"
#include "cache.h"
#include "token.h"
#include "lookup.h"
#include "macros.h"

INSTANCE_PROTOTYPES(handled);

cInstance class_registry[] = {
     INSTANCE_INIT(handled, "a frob")
};

void register_instance (InstanceID instance, Ident id) {
    class_registry[instance - FIRST_INSTANCE].id_name = id;
}

void init_instances(void) {
    register_instance (HANDLED_FROB_TYPE, frob_id);
}

/* ack, hacky */
extern cObjnum get_object_name(Ident id);

/* Effects: Returns 0 if and only if d1 and d2 are equal according to ColdC
 *	    conventions.  If d1 and d2 are of the same type and are integers or
 *	    strings, returns greater than 0 if d1 is greater than d2 according
 *	    to ColdC conventions, and less than 0 if d1 is less than d2. */
Int data_cmp(cData *d1, cData *d2) {
    if (d1->type == FLOAT && d2->type == INTEGER) {
        d2->type = FLOAT;
        d2->u.fval = (Float) d2->u.val;
    } else if (d1->type == INTEGER && d2->type == FLOAT) {
        d1->type = FLOAT;
        d1->u.fval = (Float) d1->u.val;
    }

    if (d1->type != d2->type) {
	return 1;
    }

    switch (d1->type) {

      case INTEGER:
        /* Patch #8 -- Brad Roberts, alleviate potential problems if
           MAX/MIN int are compared */
        if (d1->u.val == d2->u.val)
          return 0;
        else
          if (d1->u.val < d2->u.val)
            return -1;
          else
            return 1;

      case FLOAT: {
        Float t=d1->u.fval - d2->u.fval;
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

      default: {
	INSTANCE_RECORD(d1->type, r);
	return r->compare(d1, d2);
	}
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
	return (d->u.objnum >= 0);

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
	return 1;
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
	return hash_string_nocase(d->u.str);

      case OBJNUM:
	return d->u.objnum;

      case LIST:
	if (list_length(d->u.list) > 0)
	    return data_hash(list_first(d->u.list));
	else
	    return 100;

      case SYMBOL:
	return ident_hash(d->u.symbol);

      case T_ERROR:
	return hash_nullchar(ident_name(d->u.error));

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

    default: {
	INSTANCE_RECORD(d->type, r);
	return r->hash(d);
	}
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

      default: {
	    INSTANCE_RECORD(src->type, r);
	    r->dup(dest, src);
	}
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

      case INTEGER:
      case FLOAT:
      case OBJNUM:
	break;

      default: {
	INSTANCE_RECORD(data->type, r);
	r->discard(data);
	}
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
	return string_from_chars("<instance>",10);
    }
}

/* Effects: Returns a string containing a printed representation of data. */
cStr *data_to_literal(cData *data, int flags) {
    cStr *str = string_new(0);

    return data_add_literal_to_str(str, data, flags);
}

cStr *data_add_list_literal_to_str(cStr *str, cList *list, int flags) {
    cData *d, *next;

    str = string_addc(str, '[');
    d = list_first(list);
    if (d) {
	next = list_next(list, d);
	while (next) {
	    str = data_add_literal_to_str(str, d, flags);
	    str = string_add_chars(str, ", ", 2);
	    d = next;
	    next = list_next(list, d);
	}
	str = data_add_literal_to_str(str, d, flags);
    }
    return string_addc(str, ']');
}

/* Modifies: str (mutator, claims reference count).
 * Effects: Returns a string with the printed representation of data added to
 *	    it. */
cStr *data_add_literal_to_str(cStr *str, cData *data, int flags) {
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
          char    pre = '$';
          Obj   * obj;
          cObjnum onum;

          if (flags & DF_WITH_OBJNAMES) {
              obj = cache_retrieve(data->u.objnum);

              if (!obj || obj->objname == -1) {
                  onum = data->u.objnum;
                  if (!obj && data->u.objnum > 0 && (flags & DF_INV_OBJNUMS))
                      onum = -onum;
                  s = long_to_ascii(onum, nbuf);
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
	return data_add_list_literal_to_str(str, data->u.list, flags);

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
        str = data_add_literal_to_str(str, &d, flags);
	str = string_add_chars(str, ", ", 2);
	str = data_add_literal_to_str(str, &data->u.frob->rep, flags);
	return string_addc(str, '>');
      }

      case DICT:
	return dict_add_literal_to_str(str, data->u.dict, flags);

      case BUFFER:
	str = string_add_chars(str, "`[", 2);
	for (i = 0; i < data->u.buffer->len; i++) {
	    s = long_to_ascii(data->u.buffer->s[i], nbuf);
	    str = string_add_chars(str, s, strlen(s));
	    if (i < data->u.buffer->len - 1)
		str = string_add_chars(str, ", ", 2);
	}
	return string_addc(str, ']');

    default: {
	INSTANCE_RECORD(data->type, r);
	return r->addstr(str, data, flags);
	}
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
    default:		{ INSTANCE_RECORD(type, r); return r->id_name; }
    }
}

char * data_from_literal(cData *d, char *s) {

    while (isspace(*s))
	s++;

    d->type = -1;

    if (isdigit(*s) || ((*s == '-' || *s == '+') && isdigit(s[1]))) {
        char *t = s;

	d->type = INTEGER;
	d->u.val = (Long) atol(s);
	while (isdigit(*++s));
        if (*s=='.' || *s=='e') {
 	    d->type = FLOAT;
 	    d->u.fval = (Float) atof(t);
 	    s++;
            while (isdigit(*s) ||
                   *s == '.' ||
                   *s == 'e' ||
                   *s == '-' ||
                   *s == '+')
                s++;
 	}
	return s;
    } else if (*s == '"') {
	d->type = STRING;
	d->u.str = string_parse(&s);
	return s;
    } else if (*s == '$') {
        Ident      name;
	cObjnum    objnum;

        s++;

        name = parse_ident(&s);
        objnum = get_object_name(name);
	ident_discard(name);

	d->type = OBJNUM;
	d->u.objnum = objnum;

	return s;
    } else if (*s == '[') {
	cList *list;

	list = list_new(0);
	s++;
	while (*s && *s != ']') {
	    s = data_from_literal(d, s);
	    if (d->type == -1) {
		list_discard(list);
		d->type = -1;
		return s;
	    }
	    list = list_add(list, d);
	    data_discard(d);
	    while (isspace(*s))
		s++;
	    if (*s == ',')
		s++;
	    while (isspace(*s))
		s++;
	}
	d->type = LIST;
	d->u.list = list;
	return (*s) ? s + 1 : s;
    } else if (*s == '#' && s[1] == '[') {
	cData assocs;

	/* Get associations. */
	s = data_from_literal(&assocs, s + 1);
	if (assocs.type != LIST) {
	    if (assocs.type != -1)
		data_discard(&assocs);
	    d->type = -1;
	    return s;
	}

	/* Make a dict from the associations. */
	d->type = DICT;
	d->u.dict = dict_from_slices(assocs.u.list);
	data_discard(&assocs);
	if (!d->u.dict)
	    d->type = -1;
	return s;
    } else if (*s == '#') {
        s++;
	d->type = OBJNUM;
	d->u.objnum = (cObjnum) strtol(s, &s, 10);
	return s;
    } else if (*s == '`' && s[1] == '[') {
	cData *p, byte_data;
	cList *bytes;
	cBuf *buf;
	Int i;

	/* Get the contents of the buffer. */
	s = data_from_literal(&byte_data, s + 1);
	if (byte_data.type != LIST) {
	    if (byte_data.type != -1)
		data_discard(&byte_data);
	    return s;
	}
	bytes = byte_data.u.list;

	/* Verify that the bytes are numbers. */
	for (p = list_first(bytes); p; p = list_next(bytes, p)) {
	    if (p->type != INTEGER) {
		data_discard(&byte_data);
		return s;
	    }
	}

	/* Make a buffer from the numbers. */
	buf = buffer_new(list_length(bytes));
	i = 0;
	for (p = list_first(bytes); p; p = list_next(bytes, p))
	    buf->s[i++] = p->u.val;

	data_discard(&byte_data);
	d->type = BUFFER;
	d->u.buffer = buf;
	return s;
    } else if (*s == '\'') {
	s++;
	d->type = SYMBOL;
	d->u.symbol = parse_ident(&s);
	return s;
    } else if (*s == '~') {
	s++;
	d->type = T_ERROR;
	d->u.symbol = parse_ident(&s);
	return s;
    } else if (*s == '<') {
	cData cclass, crep;

	s = data_from_literal(&cclass, s + 1);
	if (cclass.type == OBJNUM) {
	    while (isspace(*s))
		s++;
	    if (*s == ',')
		s++;
	    while (isspace(*s))
		s++;
	    s = data_from_literal(&crep, s);
	    if (crep.type == -1) {
		d->type = -1;
		return (*s) ? s + 1 : s;
	    }
	    while (isspace(*s))
		s++;
	    if (*s == ',') {
#include "handled_frob.h"
		cData chandler;

		s++;
		while (isspace(*s))
		    s++;
		s = data_from_literal(&chandler, s);
		if (chandler.type != SYMBOL) {
		    data_discard(&crep);
		    d->type = -1;
		    if (chandler.type != -1)
			data_discard(&chandler);
		    return (*s) ? s + 1 : s;
		}
		d->type = (Int) HANDLED_FROB_TYPE;
		d->u.instance = (void*)TMALLOC(HandledFrob, 1);
		HANDLED_FROB(d)->cclass = cclass.u.objnum;
		HANDLED_FROB(d)->rep = crep;
		HANDLED_FROB(d)->handler = chandler.u.symbol;
		return (*s) ? s + 1 : s;
	    }
 	    d->type = FROB;
	    d->u.frob = TMALLOC(cFrob, 1);
	    d->u.frob->cclass = cclass.u.objnum;
	    d->u.frob->rep = crep;
	} else if (cclass.type != -1) {
	    data_discard(&cclass);
	}
	return (*s) ? s + 1 : s;
    } else {
	return (*s) ? s + 1 : s;
    }
}

