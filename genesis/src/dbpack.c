/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Write and retrieve objects to disk.
*/

#include "defs.h"

#include <string.h>
#include "cdc_db.h"
#include "macros.h"


/* Write a Float to the output buffer */
cBuf * write_float(cBuf *buf, Float f)
{
    buf = buffer_append_uchars_single_ref(buf, (uChar*)(&f), SIZEOF_FLOAT);
    return buf;
}

/* Read a Float from the input buffer */
Float read_float(cBuf *buf, Long *buf_pos)
{
    Float f;

    memcpy((uChar*)(&f), &(buf->s[*buf_pos]), SIZEOF_FLOAT);
    (*buf_pos) += SIZEOF_FLOAT;
    return f;
}

/* Determine the size of a Float */
Int size_float(Float f)
{
    return SIZEOF_FLOAT;
}

/* Write a four-byte number to fp in a consistent byte-order. */
cBuf * write_long(cBuf *buf, Long n)
{
    uLong i = (uLong)n;
    uLong i2 = i ^ (uLong)(-1);
    uChar long_buf[sizeof(Long)+1];
    Int   bit_flip = 0;
    uInt  num_bytes = 0;

    if (i2 < i) {
        i = i2;
        bit_flip = 1;
    }

    long_buf[0] = i & 15;
    i >>= 4;

    num_bytes++;
    while (i && (num_bytes <= sizeof(Long))) {
        long_buf[num_bytes++] = i & 255;
        i >>= 8;
    }
    long_buf[0] |= ((num_bytes-1) << 5) + (bit_flip << 4);
    buf = buffer_append_uchars_single_ref(buf, long_buf, num_bytes);

    return buf;
}

/* Read a four-byte number in a consistent byte-order. */
Long read_long(cBuf *buf, Long *buf_pos)
{
    Int bit_flip, num_bytes, bit_shift;
    uLong n;

    num_bytes = (unsigned)buf->s[(*buf_pos)++] & 255;
    bit_flip = num_bytes & 16;
    n = num_bytes & 15;
    bit_shift = 4;
    num_bytes >>= 5;
    while (num_bytes) {
        n += ((unsigned)buf->s[(*buf_pos)++] & 255) << bit_shift;
        bit_shift += 8;
        num_bytes--;
    }
    if (bit_flip)
        n ^= (uLong)(-1);
    return (Long)n;
}

static Int size_long_internal(Long n)
{
    uLong i = (uLong)n;
    Int num_bytes;

    i >>= 4;
    num_bytes = 0;
    while (i) {
        num_bytes++;
        i >>= 8;
    }
    return num_bytes;
}

Int size_long(Long n)
{
    uLong i = (uLong)n;
    uLong i2 = i ^ (uLong)(-1);

    if (i2 >= i)
        return size_long_internal(i);
    else
        return size_long_internal(i2);
}

cBuf * write_ident(cBuf *buf, Ident id)
{
    Char *s;
    Int len;

    if (id == NOT_AN_IDENT) {
        buf = write_long(buf, NOT_AN_IDENT);
        return buf;
    }
    s = ident_name_size(id, &len);
    buf = write_long(buf, len);
    buf = buffer_append_uchars_single_ref(buf, (uChar *)s, len);

    return buf;
}

Ident read_ident(cBuf *buf, Long *buf_pos)
{
    Int   len;
    Char *s;
    Ident id;

    /* Read the length of the identifier. */
    len = read_long(buf, buf_pos);

    /* If the length is -1, it's not really an identifier, but a -1 signalling
     * a blank variable or method. */
    if (len == NOT_AN_IDENT)
	return NOT_AN_IDENT;

    /* Otherwise, it's an identifier.  Read it into temporary storage. */
    s = TMALLOC(Char, len + 1);
    MEMCPY(s, &(buf->s[*buf_pos]), len);
    (*buf_pos) += len;
    s[len] = 0;

    /* Get the index for the identifier and free the temporary memory. */
    id = ident_get(s);
    tfree_chars(s);

    return id;
}

Int size_ident(Ident id)
{
    Int len;

    if (id == NOT_AN_IDENT)
        return size_long(NOT_AN_IDENT);

    ident_name_size(id, &len);

    return size_long(len) + (len * sizeof(Char));
}

static cBuf * pack_list(cBuf *buf, cList *list)
{
    cData *d;

    if (!list) {
        buf = write_long(buf, -1);
    } else {
        buf = write_long(buf, list_length(list));
        for (d = list_first(list); d; d = list_next(list, d))
            buf = pack_data(buf, d);
    }

    return buf;
}

static cList *unpack_list(cBuf *buf, Long *buf_pos)
{
    Int len, i;
    cList *list;
    cData *d;

    len = read_long(buf, buf_pos);
    if (len == -1) {
        list = NULL;
    } else {
        list = list_new(len);
        d = list_empty_spaces(list, len);
        for (i = 0; i < len; i++)
            unpack_data(buf, buf_pos, d++);
    }
    return list;
}

static Int size_list(cList *list)
{
    cData *d;
    Int size = 0;

    if (!list) {
        size += size_long(-1);
    } else {
        size += size_long(list_length(list));
        for (d = list_first(list); d; d = list_next(list, d))
            size += size_data(d);
    }
    return size;
}

static cBuf * pack_dict(cBuf *buf, cDict *dict)
{
    Int i;

    buf = pack_list(buf, dict->keys);
    buf = pack_list(buf, dict->values);
    if (dict->keys->len > 64) {
        buf = write_long(buf, dict->hashtab_size);
        for (i = 0; i < dict->hashtab_size; i++) {
	    buf = write_long(buf, dict->links[i]);
	    buf = write_long(buf, dict->hashtab[i]);
        }
    }
    return buf;
}

static cDict *unpack_dict(cBuf *buf, Long *buf_pos)
{
    cDict *dict;
    cList *keys, *values;
    Int i;

    keys = unpack_list(buf, buf_pos);
    values = unpack_list(buf, buf_pos);
    if (keys->len <= 64) {
        dict = dict_new(keys, values);
        list_discard(keys);
        list_discard(values);
        return dict;
    } else {
        dict = EMALLOC(cDict, 1);
        dict->keys = keys;
        dict->values = values;
        dict->hashtab_size = read_long(buf, buf_pos);
        dict->links = EMALLOC(Int, dict->hashtab_size);
        dict->hashtab = EMALLOC(Int, dict->hashtab_size);
        for (i = 0; i < dict->hashtab_size; i++) {
            dict->links[i] = read_long(buf, buf_pos);
            dict->hashtab[i] = read_long(buf, buf_pos);
        }
        dict->refs = 1;
        return dict;
    }
}

static Int size_dict(cDict *dict)
{
    Int size = 0, i;

    size += size_list(dict->keys);
    size += size_list(dict->values);
    if (dict->keys->len > 64) {
        size += size_long(dict->hashtab_size);
        for (i = 0; i < dict->hashtab_size; i++) {
	    size += size_long(dict->links[i]);
	    size += size_long(dict->hashtab[i]);
        }
    }
    return size;
}

static cBuf * pack_vars(cBuf *buf, Obj *obj)
{
    Int i;

    buf = write_long(buf, obj->vars.size);
    buf = write_long(buf, obj->vars.blanks);

    for (i = 0; i < obj->vars.size; i++) {
	buf = write_long(buf, obj->vars.hashtab[i]);
	if (obj->vars.tab[i].name != NOT_AN_IDENT) {
	    buf = write_ident(buf, obj->vars.tab[i].name);
	    buf = write_long(buf, obj->vars.tab[i].cclass);
	    buf = pack_data(buf, &obj->vars.tab[i].val);
	} else {
	    buf = write_long(buf, NOT_AN_IDENT);
	}
	buf = write_long(buf, obj->vars.tab[i].next);
    }
    return buf;
}

static void unpack_vars(cBuf *buf, Long *buf_pos, Obj *obj)
{
    Int i;

    obj->vars.size = read_long(buf, buf_pos);
    obj->vars.blanks = read_long(buf, buf_pos);

    obj->vars.hashtab = EMALLOC(Int, obj->vars.size);
    obj->vars.tab = EMALLOC(Var, obj->vars.size);

    for (i = 0; i < obj->vars.size; i++) {
	obj->vars.hashtab[i] = read_long(buf, buf_pos);
	obj->vars.tab[i].name = read_ident(buf, buf_pos);
	if (obj->vars.tab[i].name != NOT_AN_IDENT) {
	    obj->vars.tab[i].cclass = read_long(buf, buf_pos);
	    unpack_data(buf, buf_pos, &obj->vars.tab[i].val);
	}
	obj->vars.tab[i].next = read_long(buf, buf_pos);
    }

}

static Int size_vars(Obj *obj)
{
    Int size = 0, i;

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

static cBuf * pack_strings(cBuf *buf, Obj *obj)
{
    Int i;

    if (obj->methods->strings->tab_num > 0) {
#if 1
        buf = write_long(buf, obj->methods->strings->tab_size);
        buf = write_long(buf, obj->methods->strings->tab_num);
        buf = write_long(buf, obj->methods->strings->blanks);
        for (i = 0; i < obj->methods->strings->tab_size; i++) {
            buf = write_long(buf, obj->methods->strings->hashtab[i]);
            buf = write_long(buf, obj->methods->strings->tab[i].next);
            buf = write_long(buf, obj->methods->strings->tab[i].hash);
            buf = write_long(buf, obj->methods->strings->tab[i].refs);
        }
        for (i = 0; i < obj->methods->strings->tab_size; i++) {
	    buf = string_pack(buf, obj->methods->strings->tab[i].str);
        }
#else
        // caused 3 crashes on TEC, disabling code until problem can be determined
        buf = write_long(buf, obj->methods->strings->tab_size);
        for (i = 0; i < obj->methods->strings->tab_size; i++) {
            buf = string_pack(buf, obj->methods->strings->tab[i].str);
            if (obj->methods->strings->tab[i].str)
            {
                buf = write_long(buf, obj->methods->strings->tab[i].hash);
                buf = write_long(buf, obj->methods->strings->tab[i].refs);
            }
        }
#endif
    } else {
        buf = write_long(buf, -1);
    }
    return buf;
}

static void unpack_strings(cBuf *buf, Long *buf_pos, Obj *obj)
{
    Int i;
    Long size;

    size = read_long(buf, buf_pos);
    if (size != -1) {
#if 1
        obj->methods->strings = string_tab_new_with_size(size);
        obj->methods->strings->tab_num = read_long(buf, buf_pos);
        obj->methods->strings->blanks = read_long(buf, buf_pos);
        for (i = 0; i < size; i++) {
	    obj->methods->strings->hashtab[i] = read_long(buf, buf_pos);
	    obj->methods->strings->tab[i].next = read_long(buf, buf_pos);
	    obj->methods->strings->tab[i].hash = read_long(buf, buf_pos);
	    obj->methods->strings->tab[i].refs = read_long(buf, buf_pos);
        }
        for (i = 0; i < obj->methods->strings->tab_size; i++) {
	    obj->methods->strings->tab[i].str = string_unpack(buf, buf_pos);
        }
#else
        Long last_blank = -1;

        // caused 3 crashes on TEC, disabling code until problem can be determined
        obj->methods->strings = string_tab_new_with_size(size);
        obj->methods->strings->tab_size = size;
        obj->methods->strings->blanks = 0;
        for (i = 0; i < size; i++) {
            obj->methods->strings->tab[i].str = string_unpack(buf, buf_pos);
            if (obj->methods->strings->tab[i].str) {
                obj->methods->strings->tab_num++;
	        obj->methods->strings->tab[i].hash = read_long(buf, buf_pos);
                obj->methods->strings->tab[i].refs = read_long(buf, buf_pos);
                if (obj->methods->strings->blanks == i) {
                    obj->methods->strings->blanks = i+1;
                    last_blank = i+1;
                }
            } else {
                if (last_blank != -1)
                    obj->methods->strings->tab[last_blank].next = i;
                last_blank = i;
            }
        }
        string_tab_fixup_hashtab(obj->methods->strings, obj->methods->strings->tab_size);
#endif
    } else {
	obj->methods->strings = string_tab_new();
    }
}

static Int size_strings(Obj *obj)
{
    Int size = 0, i;

    if (obj->methods->strings->tab_num > 0) {
#if 1
        size += size_long(obj->methods->strings->tab_size);
        size += size_long(obj->methods->strings->tab_num);
        size += size_long(obj->methods->strings->blanks);
        for (i = 0; i < obj->methods->strings->tab_size; i++) {
	    size += size_long(obj->methods->strings->hashtab[i]);
	    size += size_long(obj->methods->strings->tab[i].next);
	    size += size_long(obj->methods->strings->tab[i].hash);
	    size += size_long(obj->methods->strings->tab[i].refs);
        }
        for (i = 0; i < obj->methods->strings->tab_size; i++) {
	    size += string_packed_size(obj->methods->strings->tab[i].str);
        }
#else
        // caused 3 crashes on TEC, disabling code until problem can be determined
        size += size_long(obj->methods->strings->tab_size);
        for (i = 0; i < obj->methods->strings->tab_size; i++) {
            size += string_packed_size(obj->methods->strings->tab[i].str);
            if (obj->methods->strings->tab[i].str)
            {
                size += size_long(obj->methods->strings->tab[i].hash);
                size += size_long(obj->methods->strings->tab[i].refs);
            }
        }
#endif
    } else {
	size += size_long(-1);
    }

    return size;
}

static cBuf * pack_idents(cBuf *buf, Obj *obj)
{
    Int i;

    buf = write_long(buf, obj->methods->idents_size);
    buf = write_long(buf, obj->methods->num_idents);
    for (i = 0; i < obj->methods->num_idents; i++) {
	if (obj->methods->idents[i].id != NOT_AN_IDENT) {
	    buf = write_ident(buf, obj->methods->idents[i].id);
	    buf = write_long(buf, obj->methods->idents[i].refs);
	} else {
	    buf = write_long(buf, NOT_AN_IDENT);
	}
    }
    return buf;
}

static void unpack_idents(cBuf *buf, Long *buf_pos, Obj *obj)
{
    Int i;

    obj->methods->idents_size = read_long(buf, buf_pos);
    obj->methods->num_idents = read_long(buf, buf_pos);
    obj->methods->idents = EMALLOC(Ident_entry, obj->methods->idents_size);
    for (i = 0; i < obj->methods->num_idents; i++) {
	obj->methods->idents[i].id = read_ident(buf, buf_pos);
	if (obj->methods->idents[i].id != NOT_AN_IDENT)
	    obj->methods->idents[i].refs = read_long(buf, buf_pos);
    }
}

static Int size_idents(Obj *obj)
{
    Int size = 0, i;

    size += size_long(obj->methods->idents_size);
    size += size_long(obj->methods->num_idents);
    for (i = 0; i < obj->methods->num_idents; i++) {
	if (obj->methods->idents[i].id != NOT_AN_IDENT) {
	    size += size_ident(obj->methods->idents[i].id);
	    size += size_long(obj->methods->idents[i].refs);
	} else {
	    size += size_long(NOT_AN_IDENT);
	}
    }

    return size;
}

static cBuf * pack_method(cBuf *buf, Method *method)
{
    Int i, j;

    buf = write_ident(buf, method->name);

    buf = write_long(buf, method->m_access);
    buf = write_long(buf, method->m_flags);
    buf = write_long(buf, method->native);

    buf = write_long(buf, method->num_args);
    for (i = 0; i < method->num_args; i++) {
	buf = write_long(buf, method->argnames[i]);
    }
    buf = write_long(buf, method->rest);

    buf = write_long(buf, method->num_vars);
    for (i = 0; i < method->num_vars; i++) {
	buf = write_long(buf, method->varnames[i]);
    }

    buf = write_long(buf, method->num_opcodes);
    for (i = 0; i < method->num_opcodes; i++)
	buf = write_long(buf, method->opcodes[i]);

    buf = write_long(buf, method->num_error_lists);
    for (i = 0; i < method->num_error_lists; i++) {
	buf = write_long(buf, method->error_lists[i].num_errors);
	for (j = 0; j < method->error_lists[i].num_errors; j++)
	    buf = write_ident(buf, method->error_lists[i].error_ids[j]);
    }
    return buf;
}

static Method *unpack_method(cBuf *buf, Long *buf_pos)
{
    Int     i, j, n;
    Method *method;
    Int     name;

    /* Read in the name.  If this is -1, it was a marker for a blank entry. */
    name = read_ident(buf, buf_pos);
    if (name == NOT_AN_IDENT)
	return NULL;

    method = EMALLOC(Method, 1);

    method->name = name;
    method->m_access = read_long(buf, buf_pos);
    method->m_flags = read_long(buf, buf_pos);
    method->native = read_long(buf, buf_pos);
    method->refs = 1;

    method->num_args = read_long(buf, buf_pos);
    if (method->num_args) {
	method->argnames = TMALLOC(Int, method->num_args);
	for (i = 0; i < method->num_args; i++) {
	    method->argnames[i] = read_long(buf, buf_pos);
        }
    }
    method->rest = read_long(buf, buf_pos);

    method->num_vars = read_long(buf, buf_pos);
    if (method->num_vars) {
	method->varnames = TMALLOC(Int, method->num_vars);
	for (i = 0; i < method->num_vars; i++) {
	    method->varnames[i] = read_long(buf, buf_pos);
        }
    }

    method->num_opcodes = read_long(buf, buf_pos);
    method->opcodes = TMALLOC(Long, method->num_opcodes);
    for (i = 0; i < method->num_opcodes; i++)
	method->opcodes[i] = read_long(buf, buf_pos);

    method->num_error_lists = read_long(buf, buf_pos);
    if (method->num_error_lists) {
	method->error_lists = TMALLOC(Error_list, method->num_error_lists);
	for (i = 0; i < method->num_error_lists; i++) {
	    n = read_long(buf, buf_pos);
	    method->error_lists[i].num_errors = n;
	    method->error_lists[i].error_ids = TMALLOC(Int, n);
	    for (j = 0; j < n; j++)
		method->error_lists[i].error_ids[j] = read_ident(buf, buf_pos);
	}
    }

    return method;
}

static Int size_method(Method *method)
{
    Int size = 0, i, j;

    size += size_ident(method->name);
    size += size_long(method->native);
    size += size_long(method->m_access);
    size += size_long(method->m_flags);

    size += size_long(method->num_args);
    for (i = 0; i < method->num_args; i++) {
	size += size_long(method->argnames[i]);
    }
    size += size_long(method->rest);

    size += size_long(method->num_vars);
    for (i = 0; i < method->num_vars; i++) {
	size += size_long(method->varnames[i]);
    }

    size += size_long(method->num_opcodes);
    for (i = 0; i < method->num_opcodes; i++)
	size += size_long(method->opcodes[i]);

    size += size_long(method->num_error_lists);
    for (i = 0; i < method->num_error_lists; i++) {
	size += size_long(method->error_lists[i].num_errors);
	for (j = 0; j < method->error_lists[i].num_errors; j++)
	    size += size_ident(method->error_lists[i].error_ids[j]);
    }

    return size;
}

static cBuf * pack_methods(cBuf *buf, Obj *obj)
{
    Int i;

    if (!object_has_methods(obj)) {
        buf = write_long(buf, -1);
        return buf;
    }

    buf = write_long(buf, obj->methods->size);
    buf = write_long(buf, obj->methods->blanks);

    for (i = 0; i < obj->methods->size; i++) {
	buf = write_long(buf, obj->methods->hashtab[i]);
	if (obj->methods->tab[i].m) {
	    buf = pack_method(buf, obj->methods->tab[i].m);
	} else {
	    /* Method begins with name identifier; write NOT_AN_IDENT. */
	    buf = write_long(buf, NOT_AN_IDENT);
	}
	buf = write_long(buf, obj->methods->tab[i].next);
    }

    buf = pack_strings(buf, obj);
    buf = pack_idents(buf, obj);

    return buf;
}

#define METHOD_STARTING_SIZE 7

static void unpack_methods(cBuf *buf, Long *buf_pos, Obj *obj)
{
    Int i, size;

    size = read_long(buf, buf_pos);

    if (size == -1) {
        obj->methods = NULL;
        return;
    }

    obj->methods = EMALLOC(ObjMethods, 1);

    obj->methods->size = size;
    obj->methods->blanks = read_long(buf, buf_pos);

    obj->methods->hashtab = EMALLOC(Int, obj->methods->size);
    obj->methods->tab = EMALLOC(struct mptr, obj->methods->size);

    for (i = 0; i < obj->methods->size; i++) {
	obj->methods->hashtab[i] = read_long(buf, buf_pos);
	obj->methods->tab[i].m = unpack_method(buf, buf_pos);
	if (obj->methods->tab[i].m)
	    obj->methods->tab[i].m->object = obj;
	obj->methods->tab[i].next = read_long(buf, buf_pos);
    }

    unpack_strings(buf, buf_pos, obj);
    unpack_idents(buf, buf_pos, obj);
}

static Int size_methods(Obj *obj)
{
    Int size = 0, i;

    if (!object_has_methods(obj)) {
        return size_long(-1);
    }

    size += size_long(obj->methods->size);
    size += size_long(obj->methods->blanks);

    for (i = 0; i < obj->methods->size; i++) {
	size += size_long(obj->methods->hashtab[i]);
	if (obj->methods->tab[i].m)
	    size += size_method(obj->methods->tab[i].m);
	else
	    size += size_long(NOT_AN_IDENT);
	size += size_long(obj->methods->tab[i].next);
    }
    
    size += size_strings(obj);
    size += size_idents(obj);

    return size;
}

cBuf * pack_data(cBuf *buf, cData *data)
{
    buf = write_long(buf, data->type);
    switch (data->type) {

      case INTEGER:
	buf = write_long(buf, data->u.val);
	break;

      case FLOAT:
	buf = write_float(buf, data->u.fval);
	break;

      case STRING:
	buf = string_pack(buf, data->u.str);
	break;

      case OBJNUM:
	buf = write_long(buf, data->u.objnum);
	break;

      case LIST:
	buf = pack_list(buf, data->u.list);
	break;

      case SYMBOL:
	buf = write_ident(buf, data->u.symbol);
	break;

      case T_ERROR:
	buf = write_ident(buf, data->u.error);
	break;

      case FROB:
	buf = write_long(buf, data->u.frob->cclass);
	buf = pack_data(buf, &data->u.frob->rep);
	break;

      case DICT:
	buf = pack_dict(buf, data->u.dict);
	break;

      case BUFFER:
	buf = write_long(buf, data->u.buffer->len);
        buf = buffer_append(buf, data->u.buffer);
	break;
      default: {
	  INSTANCE_RECORD(data->type, r);
	  buf = r->pack(buf, data);
      }
    }
    return buf;
}

void unpack_data(cBuf *buf, Long *buf_pos, cData *data)
{
    data->type = read_long(buf, buf_pos);
    switch (data->type) {

      case INTEGER:
	data->u.val = read_long(buf, buf_pos);
	break;

      case FLOAT:
	data->u.fval = read_float(buf, buf_pos);
	break;

      case STRING:
	data->u.str = string_unpack(buf, buf_pos);
	break;

      case OBJNUM:
	data->u.objnum = read_long(buf, buf_pos);
	break;

      case LIST:
	data->u.list = unpack_list(buf, buf_pos);
	break;

      case SYMBOL:
	data->u.symbol = read_ident(buf, buf_pos);
	break;

      case T_ERROR:
	data->u.error = read_ident(buf, buf_pos);
	break;

      case FROB:
	data->u.frob = TMALLOC(cFrob, 1);
	data->u.frob->cclass = read_long(buf, buf_pos);
	unpack_data(buf, buf_pos, &data->u.frob->rep);
	break;

      case DICT:
	data->u.dict = unpack_dict(buf, buf_pos);
	break;

      case BUFFER: {
	  Int len;

	  len = read_long(buf, buf_pos);
	  data->u.buffer = buffer_new(len);
          data->u.buffer->len = len;
          MEMCPY(data->u.buffer->s, &(buf->s[*buf_pos]), len);
          (*buf_pos) += len;
	  break;
      }
      default: {
	  INSTANCE_RECORD(data->type, r);
	  r->unpack(buf, buf_pos, data);
      }
    }
}

Int size_data(cData *data)
{
    Int size = 0;

    size += size_long(data->type);
    switch (data->type) {

      case INTEGER:
	size += size_long(data->u.val);
	break;

      case FLOAT:
	size += size_float(data->u.fval);
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

      case T_ERROR:
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
	  Int i;

	  size += size_long(data->u.buffer->len);
	  for (i = 0; i < data->u.buffer->len; i++)
	      size += size_long(data->u.buffer->s[i]);
	  break;
      }
      default: {
	  INSTANCE_RECORD(data->type, r);
	  size += r->size(data);
      }
    }
    return size;
}

cBuf * pack_object(cBuf *buf, Obj *obj)
{
    buf = pack_list(buf, obj->parents);
    buf = pack_list(buf, obj->children);
    buf = pack_vars(buf, obj);
    buf = pack_methods(buf, obj);
    buf = write_ident(buf, obj->objname);
    return buf;
}

void unpack_object(cBuf *buf, Long *buf_pos, Obj *obj)
{
    obj->parents = unpack_list(buf, buf_pos);
    obj->children = unpack_list(buf, buf_pos);
    unpack_vars(buf, buf_pos, obj);
    unpack_methods(buf, buf_pos, obj);
    obj->objname = read_ident(buf, buf_pos);
}

Int size_object(Obj *obj)
{
    Int size = 0;

    size = size_list(obj->parents);
    size += size_list(obj->children);
    size += size_vars(obj);
    size += size_methods(obj);
    size += size_ident(obj->objname);

    return size;
}
