/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: dbpack.c
// ---
// Write and retrieve objects to disk.
//
// note: ORDER_BYTES is buggy, don't enable it unless you are willing 
//       and able to field problems which may arise.
*/

#include "config.h"
#include "defs.h"

#include <string.h>
#include "dbpack.h"
#include "cdc_types.h"
#include "data.h"
#include "memory.h"

/* Write a four-byte number to fp in a consistent byte-order. */
void write_long(long n, FILE *fp)
{
#ifdef ORDER_BYTES
    /* Since first byte is special, special-case 0 as well. */
    if (!n) {
	putc(96, fp);
	return;
    }

    /* First byte depends on sign. */
    putc((n > 0) ? 64 + (n % 32) : 32 + (-n % 32), fp);
    n = (n > 0) ? n / 32 : -n / 32;

    while (n) {
	putc(32 + (n % 64), fp);
	n /= 64;
    }

    putc(96, fp);
#else
    fwrite(&n, sizeof(long), 1, fp);
#endif
}

/* Read a four-byte number in a consistent byte-order. */
long read_long(FILE *fp)
{
#ifdef ORDER_BYTES
    int c;
    long n, place;

    /* Check for initial terminator, meaning 0. */
    c = getc(fp);
    if (c == 96)
	return 0;

    /* Initial byte determines sign. */
    n = (c < 64) ? -((c - 32) % 32) : ((c - 64) % 32);
    place = (c < 64) ? -32 : 32;

    while (1) {
	c = getc(fp);
	if (c == 96)
	    return n;
	n += place * (c - 32);
	place *= 64;
    }
#else
    long    l;
  
    fread(&l, sizeof(long), 1, fp);

    return l;
#endif
}

int size_long(long n)
{
#ifdef ORDER_BYTES
    int count = 2;

    if (!n)
	return 1;
    n /= 32;
    while (n) {
	n /= 64;
	count++;
    }
    return count;
#else
    return sizeof(n);
#endif
}


INTERNAL void write_ident(long id, FILE *fp)
{
    char *s;
    int len;

    if (id == NOT_AN_IDENT) {
        write_long(NOT_AN_IDENT, fp);
        return;
    }
    s = ident_name(id);
    len = strlen(s);
    write_long(len, fp);
    fwrite(s, sizeof(char), len, fp);
}

INTERNAL long read_ident(FILE *fp)
{
    int len;
    char *s;
    long id;

    /* Read the length of the identifier. */
    len = read_long(fp);

    /* If the length is -1, it's not really an identifier, but a -1 signalling
     * a blank variable or method. */
    if (len == NOT_AN_IDENT)
	return NOT_AN_IDENT;

    /* Otherwise, it's an identifier.  Read it into temporary storage. */
    s = TMALLOC(char, len + 1);
    fread(s, sizeof(char), len, fp);
    s[len] = 0;


    /* Get the index for the identifier and free the temporary memory. */
    id = ident_get(s);
    tfree_chars(s);

    return id;
}

INTERNAL long size_ident(long id)
{
    int len = strlen(ident_name(id));

    return size_long(len) + (len * sizeof(char));
}

/* forward references for recursion */
INTERNAL void pack_data(data_t *data, FILE *fp);
INTERNAL void unpack_data(data_t *data, FILE *fp);

INTERNAL void pack_list(list_t *list, FILE *fp) {
    data_t *d;

    write_long(list_length(list), fp);
    for (d = list_first(list); d; d = list_next(list, d))
	pack_data(d, fp);
}

INTERNAL list_t *unpack_list(FILE *fp) {
    int len, i;
    list_t *list;
    data_t *d;

    len = read_long(fp);
    list = list_new(len);
    d = list_empty_spaces(list, len);
    for (i = 0; i < len; i++)
	unpack_data(d++, fp);
    return list;
}

INTERNAL int size_list(list_t *list)
{
    data_t *d;
    int size = 0;

    size += size_long(list_length(list));
    for (d = list_first(list); d; d = list_next(list, d))
	size += size_data(d);
    return size;
}

INTERNAL void pack_dict(Dict *dict, FILE *fp)
{
    int i;

    pack_list(dict->keys, fp);
    pack_list(dict->values, fp);
    write_long(dict->hashtab_size, fp);
    for (i = 0; i < dict->hashtab_size; i++) {
	write_long(dict->links[i], fp);
	write_long(dict->hashtab[i], fp);
    }
}

INTERNAL Dict *unpack_dict(FILE *fp)
{
    Dict *dict;
    int i;

    dict = EMALLOC(Dict, 1);
    dict->keys = unpack_list(fp);
    dict->values = unpack_list(fp);
    dict->hashtab_size = read_long(fp);
    dict->links = EMALLOC(int, dict->hashtab_size);
    dict->hashtab = EMALLOC(int, dict->hashtab_size);
    for (i = 0; i < dict->hashtab_size; i++) {
	dict->links[i] = read_long(fp);
	dict->hashtab[i] = read_long(fp);
    }
    dict->refs = 1;
    return dict;
}

INTERNAL int size_dict(Dict *dict)
{
    int size = 0, i;

    size += size_list(dict->keys);
    size += size_list(dict->values);
    size += size_long(dict->hashtab_size);
    for (i = 0; i < dict->hashtab_size; i++) {
	size += size_long(dict->links[i]);
	size += size_long(dict->hashtab[i]);
    }
    return size;
}

INTERNAL void pack_vars(object_t *obj, FILE *fp)
{
    int i;

    write_long(obj->vars.size, fp);
    write_long(obj->vars.blanks, fp);

    for (i = 0; i < obj->vars.size; i++) {
	write_long(obj->vars.hashtab[i], fp);
	if (obj->vars.tab[i].name != NOT_AN_IDENT) {
	    write_ident(obj->vars.tab[i].name, fp);
	    write_long(obj->vars.tab[i].cclass, fp);
	    pack_data(&obj->vars.tab[i].val, fp);
	} else {
	    write_long(NOT_AN_IDENT, fp);
	}
	write_long(obj->vars.tab[i].next, fp);
    }
}

INTERNAL void unpack_vars(object_t *obj, FILE *fp)
{
    int i;

    obj->vars.size = read_long(fp);
    obj->vars.blanks = read_long(fp);

    obj->vars.hashtab = EMALLOC(int, obj->vars.size);
    obj->vars.tab = EMALLOC(Var, obj->vars.size);

    for (i = 0; i < obj->vars.size; i++) {
	obj->vars.hashtab[i] = read_long(fp);
	obj->vars.tab[i].name = read_ident(fp);
	if (obj->vars.tab[i].name != NOT_AN_IDENT) {
	    obj->vars.tab[i].cclass = read_long(fp);
	    unpack_data(&obj->vars.tab[i].val, fp);
	}
	obj->vars.tab[i].next = read_long(fp);
    }

}

INTERNAL int size_vars(object_t *obj)
{
    int size = 0, i;

    size += size_long(obj->vars.size);
    size += size_long(obj->vars.blanks);

    for (i = 0; i < obj->vars.size; i++) {
	size += size_long(obj->vars.hashtab[i]);
	if (obj->vars.tab[i].name != NOT_AN_IDENT) {
	    size += size_ident(obj->vars.tab[i].name);
	    size += size_long(obj->vars.tab[i].cclass);
	    size += size_data(&obj->vars.tab[i].val);
	} else {
	    size += size_long(NOT_AN_IDENT);
	}
	size += size_long(obj->vars.tab[i].next);
    }

    return size;
}

INTERNAL void pack_method(method_t *method, FILE *fp)
{
    int i, j;

    write_ident(method->name, fp);

    write_long(method->num_args, fp);
    for (i = 0; i < method->num_args; i++)
	write_long(method->argnames[i], fp);
    write_long(method->rest, fp);

    write_long(method->num_vars, fp);
    for (i = 0; i < method->num_vars; i++)
	write_long(method->varnames[i], fp);

    write_long(method->num_opcodes, fp);
    for (i = 0; i < method->num_opcodes; i++)
	write_long(method->opcodes[i], fp);

    write_long(method->num_error_lists, fp);
    for (i = 0; i < method->num_error_lists; i++) {
	write_long(method->error_lists[i].num_errors, fp);
	for (j = 0; j < method->error_lists[i].num_errors; j++)
	    write_ident(method->error_lists[i].error_ids[j], fp);
    }

    write_long(method->m_access, fp);
    write_long(method->m_flags, fp);
    write_long(method->native, fp);
}

INTERNAL method_t *unpack_method(FILE *fp)
{
    int name, i, j, n;
    method_t *method;

    /* Read in the name.  If this is -1, it was a marker for a blank entry. */
    name = read_ident(fp);
    if (name == NOT_AN_IDENT)
	return NULL;

    method = EMALLOC(method_t, 1);

    method->name = name;

    method->num_args = read_long(fp);
    if (method->num_args) {
	method->argnames = TMALLOC(int, method->num_args);
	for (i = 0; i < method->num_args; i++)
	    method->argnames[i] = read_long(fp);
    }
    method->rest = read_long(fp);

    method->num_vars = read_long(fp);
    if (method->num_vars) {
	method->varnames = TMALLOC(int, method->num_vars);
	for (i = 0; i < method->num_vars; i++)
	    method->varnames[i] = read_long(fp);
    }

    method->num_opcodes = read_long(fp);
    method->opcodes = TMALLOC(long, method->num_opcodes);
    for (i = 0; i < method->num_opcodes; i++)
	method->opcodes[i] = read_long(fp);

    method->num_error_lists = read_long(fp);
    if (method->num_error_lists) {
	method->error_lists = TMALLOC(Error_list, method->num_error_lists);
	for (i = 0; i < method->num_error_lists; i++) {
	    n = read_long(fp);
	    method->error_lists[i].num_errors = n;
	    method->error_lists[i].error_ids = TMALLOC(int, n);
	    for (j = 0; j < n; j++)
		method->error_lists[i].error_ids[j] = read_ident(fp);
	}
    }

    method->m_access = read_long(fp);
    method->m_flags = read_long(fp);
    method->native = read_long(fp);

    method->refs = 1;
    return method;
}

INTERNAL int size_method(method_t *method)
{
    int size = 0, i, j;

    size += size_ident(method->name);

    size += size_long(method->num_args);
    for (i = 0; i < method->num_args; i++)
	size += size_long(method->argnames[i]);
    size += size_long(method->rest);

    size += size_long(method->num_vars);
    for (i = 0; i < method->num_vars; i++)
	size += size_long(method->varnames[i]);

    size += size_long(method->num_opcodes);
    for (i = 0; i < method->num_opcodes; i++)
	size += size_long(method->opcodes[i]);

    size += size_long(method->num_error_lists);
    for (i = 0; i < method->num_error_lists; i++) {
	size += size_long(method->error_lists[i].num_errors);
	for (j = 0; j < method->error_lists[i].num_errors; j++)
	    size += size_ident(method->error_lists[i].error_ids[j]);
    }

    size += size_long(method->native);
    size += size_long(method->m_access);
    size += size_long(method->m_flags);
    return size;
}

INTERNAL void pack_methods(object_t *obj, FILE *fp)
{
    int i;

    write_long(obj->methods.size, fp);
    write_long(obj->methods.blanks, fp);

    for (i = 0; i < obj->methods.size; i++) {
	write_long(obj->methods.hashtab[i], fp);
	if (obj->methods.tab[i].m) {
	    pack_method(obj->methods.tab[i].m, fp);
	} else {
	    /* Method begins with name identifier; write NOT_AN_IDENT. */
	    write_long(NOT_AN_IDENT, fp);
	}
	write_long(obj->methods.tab[i].next, fp);
    }
}

INTERNAL void unpack_methods(object_t *obj, FILE *fp)
{
    int i;

    obj->methods.size = read_long(fp);
    obj->methods.blanks = read_long(fp);

    obj->methods.hashtab = EMALLOC(int, obj->methods.size);
    obj->methods.tab = EMALLOC(struct mptr, obj->methods.size);

    for (i = 0; i < obj->methods.size; i++) {
	obj->methods.hashtab[i] = read_long(fp);
	obj->methods.tab[i].m = unpack_method(fp);
	if (obj->methods.tab[i].m)
	    obj->methods.tab[i].m->object = obj;
	obj->methods.tab[i].next = read_long(fp);
    }
}

INTERNAL int size_methods(object_t *obj)
{
    int size = 0, i;

    size += size_long(obj->methods.size);
    size += size_long(obj->methods.blanks);

    for (i = 0; i < obj->methods.size; i++) {
	size += size_long(obj->methods.hashtab[i]);
	if (obj->methods.tab[i].m)
	    size += size_method(obj->methods.tab[i].m);
	else
	    size += size_long(NOT_AN_IDENT);
	size += size_long(obj->methods.tab[i].next);
    }

    return size;
}

INTERNAL void pack_strings(object_t *obj, FILE *fp)
{
    int i;

    write_long(obj->strings_size, fp);
    write_long(obj->num_strings, fp);
    for (i = 0; i < obj->num_strings; i++) {
	string_pack(obj->strings[i].str, fp);
	if (obj->strings[i].str)
	    write_long(obj->strings[i].refs, fp);
    }
}

INTERNAL void unpack_strings(object_t *obj, FILE *fp)
{
    int i;

    obj->strings_size = read_long(fp);
    obj->num_strings = read_long(fp);
    obj->strings = EMALLOC(String_entry, obj->strings_size);
    for (i = 0; i < obj->num_strings; i++) {
	obj->strings[i].str = string_unpack(fp);
	if (obj->strings[i].str)
	    obj->strings[i].refs = read_long(fp);
    }
}

INTERNAL int size_strings(object_t *obj)
{
    int size = 0, i;

    size += size_long(obj->strings_size);
    size += size_long(obj->num_strings);
    for (i = 0; i < obj->num_strings; i++) {
	size += string_packed_size(obj->strings[i].str);
	if (obj->strings[i].str)
	    size += size_long(obj->strings[i].refs);
    }

    return size;
}

INTERNAL void pack_idents(object_t *obj, FILE *fp)
{
    int i;

    write_long(obj->idents_size, fp);
    write_long(obj->num_idents, fp);
    for (i = 0; i < obj->num_idents; i++) {
	if (obj->idents[i].id != NOT_AN_IDENT) {
	    write_ident(obj->idents[i].id, fp);
	    write_long(obj->idents[i].refs, fp);
	} else {
	    write_long(NOT_AN_IDENT, fp);
	}
    }
}

INTERNAL void unpack_idents(object_t *obj, FILE *fp)
{
    int i;

    obj->idents_size = read_long(fp);
    obj->num_idents = read_long(fp);
    obj->idents = EMALLOC(Ident_entry, obj->idents_size);
    for (i = 0; i < obj->num_idents; i++) {
	obj->idents[i].id = read_ident(fp);
	if (obj->idents[i].id != NOT_AN_IDENT)
	    obj->idents[i].refs = read_long(fp);
    }
}

INTERNAL int size_idents(object_t *obj)
{
    int size = 0, i;

    size += size_long(obj->idents_size);
    size += size_long(obj->num_idents);
    for (i = 0; i < obj->num_idents; i++) {
	if (obj->idents[i].id != NOT_AN_IDENT) {
	    size += size_ident(obj->idents[i].id);
	    size += size_long(obj->idents[i].refs);
	} else {
	    size += size_long(NOT_AN_IDENT);
	}
    }

    return size;
}

INTERNAL void pack_data(data_t *data, FILE *fp)
{
    write_long(data->type, fp);
    switch (data->type) {

      case INTEGER:
	write_long(data->u.val, fp);
	break;

      case FLOAT:
        write_long(*((long*)(&data->u.fval)), fp);
        break;

      case STRING:
	string_pack(data->u.str, fp);
	break;

      case OBJNUM:
	write_long(data->u.objnum, fp);
	break;

      case LIST:
	pack_list(data->u.list, fp);
	break;

      case SYMBOL:
	write_ident(data->u.symbol, fp);
	break;

      case ERROR:
	write_ident(data->u.error, fp);
	break;

      case FROB:
	write_long(data->u.frob->cclass, fp);
	pack_data(&data->u.frob->rep, fp);
	break;

      case DICT:
	pack_dict(data->u.dict, fp);
	break;

      case BUFFER: {
	  int i;

	  write_long(data->u.buffer->len, fp);
	  for (i = 0; i < data->u.buffer->len; i++)
	      write_long(data->u.buffer->s[i], fp);
	  break;
      }
    }
}

INTERNAL void unpack_data(data_t *data, FILE *fp)
{
    data->type = read_long(fp);
    switch (data->type) {

      case INTEGER:
	data->u.val = read_long(fp);
	break;

      case FLOAT: {
        long k = read_long(fp);
        data->u.fval = *((float*)(&k));
        break;
      }

      case STRING:
	data->u.str = string_unpack(fp);
	break;

      case OBJNUM:
	data->u.objnum = read_long(fp);
	break;

      case LIST:
	data->u.list = unpack_list(fp);
	break;

      case SYMBOL:
	data->u.symbol = read_ident(fp);
	break;

      case ERROR:
	data->u.error = read_ident(fp);
	break;

      case FROB:
        data->u.frob = TMALLOC(Frob, 1);
	data->u.frob->cclass = read_long(fp);
	unpack_data(&data->u.frob->rep, fp);
	break;

      case DICT:
	data->u.dict = unpack_dict(fp);
	break;

      case BUFFER: {
	  int len, i;

	  len = read_long(fp);
	  data->u.buffer = buffer_new(len);
	  for (i = 0; i < len; i++)
	      data->u.buffer->s[i] = read_long(fp);
	  break;
      }
    }
}

int size_data(data_t *data) {
    int size = 0;

    size += size_long(data->type);
    switch (data->type) {

      case INTEGER:
	size += size_long(data->u.val);
	break;

      case FLOAT:
        size += size_long(*((float*)(&data->u.fval)));
        break;

      case STRING:
	size += string_packed_size(data->u.str);
	break;

      case OBJNUM:
	size += size_long(data->u.objnum);
	break;

      case LIST:
	size += size_list(data->u.list);
	break;

      case SYMBOL:
	size += size_ident(data->u.symbol);
	break;

      case ERROR:
	size += size_ident(data->u.error);
	break;

      case FROB:
	size += size_long(data->u.frob->cclass);
	size += size_data(&data->u.frob->rep);
	break;

      case DICT:
	size += size_dict(data->u.dict);
	break;

      case BUFFER: {
	  int i;

	  size += size_long(data->u.buffer->len);
	  for (i = 0; i < data->u.buffer->len; i++)
	      size += size_long(data->u.buffer->s[i]);
	  break;
      }
    }

    return size;
}

void pack_object(object_t *obj, FILE *fp)
{
    pack_list(obj->parents, fp);
    pack_list(obj->children, fp);
    pack_vars(obj, fp);
    pack_methods(obj, fp);
    pack_strings(obj, fp);
    pack_idents(obj, fp);
    write_ident(obj->objname, fp);
    write_long(obj->search, fp);
}

void unpack_object(object_t *obj, FILE *fp)
{
    obj->parents = unpack_list(fp);
    obj->children = unpack_list(fp);
    unpack_vars(obj, fp);
    unpack_methods(obj, fp);
    unpack_strings(obj, fp);
    unpack_idents(obj, fp);
    obj->objname = read_ident(fp);
    obj->search = read_long(fp);
}

int size_object(object_t *obj)
{
    int size = 0;

    size = size_list(obj->parents);
    size += size_list(obj->children);
    size += size_vars(obj);
    size += size_methods(obj);
    size += size_strings(obj);
    size += size_idents(obj);
    if (obj->objname != -1)
        size += size_ident(obj->objname);
    size += size_long(obj->search);
    return size;
}

