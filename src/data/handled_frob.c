
#include "defs.h"

#include <string.h>
#include "cdc_pcode.h"
#include "operators.h"
#include "execute.h"
#include "lookup.h"
#include "util.h"
#include "data.h"
#include "dbpack.h"

#include "handled_frob.h"

INSTANCE_PROTOTYPES(handled);

cBuf *pack_handled (cBuf *buf, const cData *d)
{
    HandledFrob *h = HANDLED_FROB(d);

    buf = write_long (buf, h->cclass);
    buf = pack_data  (buf, &h->rep);
    buf = write_ident(buf, h->handler);
    return buf;
}

void unpack_handled (const cBuf *buf, Long *buf_pos, cData *d)
{
    HandledFrob *h=TMALLOC(HandledFrob, 1);

    h->cclass = read_long(buf, buf_pos);
    unpack_data (buf, buf_pos, &h->rep);
    h->handler = read_ident(buf, buf_pos);
    d->u.instance = (void*) h;
}

int size_handled (const cData *d, int memory_size)
{
    HandledFrob *h = HANDLED_FROB(d);
    Int size = 0;

    size += size_long(h->cclass, memory_size);
    size += size_data(&h->rep, memory_size);
    size += size_ident(h->handler, memory_size);
    return size;
}

int compare_handled (cData *d1, cData *d2)
{
    HandledFrob *h1 = HANDLED_FROB(d1),
                *h2 = HANDLED_FROB(d2);

    if (h1->cclass != h2->cclass ||
        h1->handler != h2->handler)
        return 1;

    return data_cmp(&h1->rep, &h2->rep);
}

int hash_handled (const cData *d)
{
    HandledFrob *h = HANDLED_FROB(d);

    return h->cclass + h->handler + data_hash(&h->rep);
}

void dup_handled (cData *dest, const cData *source)
{
    HandledFrob *s = HANDLED_FROB(source),
                *d = TMALLOC(HandledFrob, 1);

    d->cclass = s->cclass;
    d->handler = ident_dup(s->handler);
    data_dup (&d->rep, &s->rep);
    dest->u.instance = d;
}

void discard_handled (cData *d)
{
    HandledFrob *h = HANDLED_FROB(d);

    data_discard(&h->rep);
    ident_discard(h->handler);
}

cStr *string_handled (cStr *str, const cData *data, int flags)
{
    HandledFrob *h = HANDLED_FROB(data);
    cData d;

    str = string_addc (str, '<');
    d.type = OBJNUM;
    d.u.objnum = h->cclass;
    str = data_add_literal_to_str(str, &d, flags);
    str = string_add_chars(str, ", ", 2);
    str = data_add_literal_to_str(str, &h->rep, flags);
    str = string_add_chars(str, ", ", 2);
    d.type = SYMBOL;
    d.u.symbol = h->handler;
    str = data_add_literal_to_str(str, &d, flags);
    return string_addc(str, '>');
}
