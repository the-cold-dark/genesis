/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: data.c
// ---
// Routines for ColdC data manipulation.
*/

#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "cdc_types.h"
#include "data.h"
#include "util.h"
#include "cache.h"
#include "memory.h"
#include "token.h"
#include "log.h"
#include "lookup.h"

/* Effects: Returns 0 if and only if d1 and d2 are equal according to ColdC
 *	    conventions.  If d1 and d2 are of the same type and are integers or
 *	    strings, returns greater than 0 if d1 is greater than d2 according
 *	    to ColdC conventions, and less than 0 if d1 is less than d2. */
int data_cmp(data_t *d1, data_t *d2) {
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

      case DBREF:
	return (d1->u.dbref != d2->u.dbref);

      case LIST:
	return list_cmp(d1->u.list, d2->u.list);

      case SYMBOL:
	return (d1->u.symbol != d2->u.symbol);

      case ERROR:
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
int data_true(data_t *d)
{
    switch (d->type) {

      case INTEGER:
	return (d->u.val != 0);

      case FLOAT:
        return (d->u.fval != 0.0);

      case STRING:
	return (string_length(d->u.str) != 0);

      case DBREF:
	return 1;

      case LIST:
	return (list_length(d->u.list) != 0);

      case SYMBOL:
	return 1;

      case ERROR:
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

unsigned long data_hash(data_t *d)
{
    list_t *values;

    switch (d->type) {

      case INTEGER:
	return d->u.val;

      case FLOAT:
        return *((long*)(&d->u.fval));

      case STRING:
	return hash_case(string_chars(d->u.str), string_length(d->u.str));

      case DBREF:
	return d->u.dbref;

      case LIST:
	if (list_length(d->u.list) > 0)
	    return data_hash(list_first(d->u.list));
	else
	    return 100;

      case SYMBOL:
	return hash(ident_name(d->u.symbol));

      case ERROR:
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
void data_dup(data_t *dest, data_t *src)
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

      case DBREF:
	dest->u.dbref = src->u.dbref;
	break;

      case LIST:
	dest->u.list = list_dup(src->u.list);
	break;

      case SYMBOL:
	dest->u.symbol = ident_dup(src->u.symbol);
	break;

      case ERROR:
	dest->u.error = ident_dup(src->u.error);
	break;

      case FROB:
	dest->u.frob = TMALLOC(Frob, 1);
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
void data_discard(data_t *data)
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

      case ERROR:
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

string_t *data_tostr(data_t *data)
{
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

      case DBREF:
	s = long_to_ascii(data->u.dbref, nbuf);
	return string_add_chars(string_from_chars("#", 1), s, strlen(s));

      case LIST:
	return string_from_chars("[list]", 6);

      case SYMBOL:
	s = ident_name(data->u.symbol);
	return string_from_chars(s, strlen(s));

      case ERROR:
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
string_t *data_to_literal(data_t *data)
{
    string_t *str = string_new(0);

    return data_add_literal_to_str(str, data);
}

string_t *data_add_list_literal_to_str(string_t *str, list_t *list)
{
    data_t *d, *next;

    str = string_addc(str, '[');
    d = list_first(list);
    if (d) {
	next = list_next(list, d);
	while (next) {
	    str = data_add_literal_to_str(str, d);
	    str = string_add_chars(str, ", ", 2);
	    d = next;
	    next = list_next(list, d);
	}
	str = data_add_literal_to_str(str, d);
    }
    return string_addc(str, ']');
}

/* Modifies: str (mutator, claims reference count).
 * Effects: Returns a string with the printed representation of data added to
 *	    it. */
string_t *data_add_literal_to_str(string_t *str, data_t *data)
{
    char *s;
    Number_buf nbuf;
    int i;

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

      case DBREF:
	s = long_to_ascii(data->u.dbref, nbuf);
	str = string_addc(str, '#');
	return string_add_chars(str, s, strlen(s));

      case LIST:
	return data_add_list_literal_to_str(str, data->u.list);

      case SYMBOL:
	str = string_addc(str, '\'');
	s = ident_name(data->u.symbol);
	if (*s && is_valid_ident(s))
	    return string_add_chars(str, s, strlen(s));
	else
	    return string_add_unparsed(str, s, strlen(s));

      case ERROR:
	str = string_addc(str, '~');
	s = ident_name(data->u.error);
	if (is_valid_ident(s))
	    return string_add_chars(str, s, strlen(s));
	else
	    return string_add_unparsed(str, s, strlen(s));

      case FROB:
	str = string_add_chars(str, "<#", 2);
	s = long_to_ascii(data->u.frob->cclass, nbuf);
	str = string_add_chars(str, s, strlen(s));
	str = string_add_chars(str, ", ", 2);
	str = data_add_literal_to_str(str, &data->u.frob->rep);
	return string_addc(str, '>');

      case DICT:
	return dict_add_literal_to_str(str, data->u.dict);

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

char *data_from_literal(data_t *d, char *s)
{
    while (isspace(*s))
	s++;

    d->type = -1;

    if (isdigit(*s) || (*s == '-' && isdigit(s[1]))) {
        char *t = s;

	d->type = INTEGER;
	d->u.val = atol(s);
	while (isdigit(*++s));
        if (*s=='.' || *s=='e') {
 	    d->type = FLOAT;
 	    d->u.fval = atof(t);
 	    s++;
            while (isdigit(*s) || *s == '.' || *s == 'e' || *s == '-') s++;
 	}
	return s;
    } else if (*s == '"') {
	d->type = STRING;
	d->u.str = string_parse(&s);
	return s;
    } else if (*s == '#' && (isdigit(s[1]) || s[1] == '-')) {
	d->type = DBREF;
	d->u.dbref = atol(++s);
	while (isdigit(*++s));
	return s;
    } else if (*s == '$') {
	long name, dbref;

	s++;
	name = parse_ident(&s);
	if (!lookup_retrieve_name(name, &dbref))
	    dbref = -1;
	ident_discard(name);
	d->type = DBREF;
	d->u.dbref = dbref;
	return s;
    } else if (*s == '[') {
	list_t *list;

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
	data_t assocs;

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
    } else if (*s == '`' && s[1] == '[') {
	data_t *p, byte_data;
	list_t *bytes;
	Buffer *buf;
	int i;

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
	d->type = ERROR;
	d->u.symbol = parse_ident(&s);
	return s;
    } else if (*s == '<') {
	data_t cclass;

	s = data_from_literal(&cclass, s + 1);
	if (cclass.type == DBREF) {
	    while (isspace(*s))
		s++;
	    if (*s == ',')
		s++;
	    while (isspace(*s))
		s++;
	    d->type = FROB;
	    d->u.frob = TMALLOC(Frob, 1);
	    d->u.frob->cclass = cclass.u.dbref;
	    s = data_from_literal(&d->u.frob->rep, s);
	    if (d->u.frob->rep.type == -1) {
		TFREE(d->u.frob, 1);
		d->type = -1;
	    }
	} else if (cclass.type != -1) {
	    data_discard(&cclass);
	}
	return (*s) ? s + 1 : s;
    } else {
	return (*s) ? s + 1 : s;
    }
}

/* Effects: Returns an id (without updating reference count) for the name of
 *	    the type given by type. */
long data_type_id(int type)
{
    switch (type) {
      case INTEGER:	return integer_id;
      case FLOAT:	return float_id;
      case STRING:	return string_id;
      case DBREF:	return dbref_id;
      case LIST:	return list_id;
      case SYMBOL:	return symbol_id;
      case ERROR:	return error_id;
      case FROB:	return frob_id;
      case DICT:	return dictionary_id;
      case BUFFER:	return buffer_id;
      default:		panic("Unrecognized data type."); return 0;
    }
}

