/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/veil.c
// ---
// Veil packets buffer manipulation module
*/

#define NATIVE_MODULE "$buffer"

#include "config.h"
#include "defs.h"
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"
#include "memory.h"
#include "veil.h"
#include "native.h"

module_t veil_module = {init_veil, uninit_veil};

extern Ident pabort_id, pclose_id, popen_id;

#define VEIL_P_PUSH      1
#define VEIL_P_ABORT     2
#define VEIL_P_CLOSE     4
#define VEIL_P_OPEN      8
#define VEIL_P_UNDF1     16
#define VEIL_P_UNDF2     32
#define VEIL_P_UNDF3     64
#define VEIL_P_UNDF4     128

/*
// 1 byte bitfield flags
// 1 byte unused
// 2 bytes session id
// 2 bytes unused
// 2 bytes length
*/

#define HEADER_SIZE    8

#ifdef UNDEFINED
/* using the standard way was causing unaligned accesses */
typedef struct header_s {
    unsigned char  flags;
    unsigned char  u0;
    unsigned short sid;
    unsigned char  u1[2];
    unsigned short len;
} header_t;
#endif

void init_veil(int argc, char ** argv) {
    pabort_id = ident_get("abort");
    pclose_id = ident_get("close");
    popen_id  = ident_get("open");
}

void uninit_veil(void) {
    ident_discard(pabort_id);
    ident_discard(pclose_id);
    ident_discard(popen_id);
}

/*
// -------------------------------------------------------------------
// In ColdC we represent a VEIL packet as:
//
//       [1|0, 'abort|'close|'open, id, `[]]
//
// The first argument represents the push bit, and the end of a message.
// id is an integer.
*/

/*
// -------------------------------------------------------------------
// internal function for buffer -> VEIL packet
*/
list_t * buffer_to_veil_packets(Buffer * buf) {
    int             flags,
                    session,
                    length,
                    blen;
    Buffer        * databuf,
                  * incomplete;
    data_t          d,
                  * list;
    list_t        * output,
                  * packet;
    unsigned char * cbuf;

    output = list_new(0);
    blen = buf->len;
    cbuf = buf->s;

    for (;;) {
        /* 8 bytes will give us the header, we check length again after
           that, because the header gives us the data length */
        if (blen < HEADER_SIZE)
            break;

        flags = (int) cbuf[0];
        session = (int) cbuf[2] * 256 + (int) cbuf[3];
        length = (int) cbuf[6] * 256 + (int) cbuf[7];

        if (length > (blen - HEADER_SIZE))
            break;

        /* copy the data segment of the packet to the databuf */
        databuf = buffer_new(length);
        cbuf += HEADER_SIZE;
        MEMCPY(databuf->s, cbuf, length);
        cbuf += (length);
        blen -= (length + HEADER_SIZE);

        /* create the packet list, add it to the output list */

        packet = list_new(4);
        list = list_empty_spaces(packet, 4);
        list[0].type = INTEGER;
        list[0].u.val = (flags & VEIL_P_PUSH) ? 1 : 0;

        /* give abort precedence */
        if (flags & VEIL_P_ABORT) {
            list[1].type     = SYMBOL;
            list[1].u.symbol = pabort_id;
        } else if (flags & VEIL_P_CLOSE) {
            list[1].type     = SYMBOL;
            list[1].u.symbol = pclose_id;
        } else if (flags & VEIL_P_OPEN) {
            list[1].type     = SYMBOL;
            list[1].u.symbol = popen_id;
        } else {
            list[1].type  = INTEGER;
            list[1].u.val = 0;
        }

        list[2].type = INTEGER;
        list[2].u.val = session;

        list[3].type = BUFFER;
        list[3].u.buffer = buffer_dup(databuf);

        /* add it to the list */
        d.type = LIST;
        d.u.list = packet;
        output = list_add(output, &d);

        buffer_discard(databuf);
        list_discard(packet);
    }

    /* add the incomplete buffer to the end */
    incomplete = buffer_new(blen);
    if (blen > 0)
        MEMMOVE(incomplete->s, cbuf, blen);
    d.type = BUFFER;
    d.u.buffer = incomplete;
    output = list_add(output, &d);
    buffer_discard(incomplete);

    return output;
}

/*
// -------------------------------------------------------------------
// Convert from a buffer to the standard 'VEIL packet' format
*/
NATIVE_METHOD(to_veil_pkts) {
    Buffer * buf;
    list_t * packets;

    INIT_1_OR_2_ARGS(BUFFER, BUFFER);

    if (argc == 1) {
        buf = buffer_dup(_BUF(ARG1));
    } else {
        /* if they sent us a second argument, concatenate it on the end */
        buf = buffer_append(buffer_dup(_BUF(ARG1)), _BUF(ARG2));
    }

    packets = buffer_to_veil_packets(buf);

    buffer_discard(buf);

    RETURN_LIST(packets);
}

/*
// -------------------------------------------------------------------
// Convert to a buffer from the standard 'VEIL packet' format
//
// in leiu of speed, we do not do COMPLETE checking on every argument
// sent to the packet, just make sure you do it right
*/
NATIVE_METHOD(from_veil_pkts) {
    data_t        * d,
                  * pa;
    Buffer        * out,
                  * header;
    list_t        * p, * packets;
    int             len;

    INIT_1_ARG(LIST);

    header = buffer_new(HEADER_SIZE);
    header->s[1] = (unsigned char) 0;
    header->s[4] = (unsigned char) 0;
    header->s[5] = (unsigned char) 0;

    out = buffer_new(0);
    packets = _LIST(ARG1);

    for (d = list_first(packets); d; d = list_next(packets, d)) {
        if (d->type != LIST)
            THROW((type_id, "Packets submitted in an invalid format"));
        p = d->u.list;
        pa = list_first(p);
        if (!pa || pa->type != INTEGER)
            THROW((type_id, "Packet submitted in an invalid format"));

        header->s[0] = (unsigned char) 0;
        if (pa->u.val)
            header->s[0] |= VEIL_P_PUSH;

        pa = list_next(p, pa);
        if (!pa)
            THROW((type_id, "Not enough elements in the packet!"));
        if (pa->type == SYMBOL) {
            if (pa->u.symbol == pabort_id)
                header->s[0] |= VEIL_P_ABORT;
            else if (pa->u.symbol == pclose_id)
                header->s[0] |= VEIL_P_CLOSE;
            else if (pa->u.symbol == popen_id)
                header->s[0] |= VEIL_P_OPEN;
        }

        pa = list_next(p, pa);
        if (!pa || pa->type != INTEGER)
            THROW((type_id, "Packet submitted in an invalid format"));

        header->s[2] = (unsigned char) pa->u.val/256;
        header->s[3] = (unsigned char) pa->u.val%256;

        pa = list_next(p, pa);
        if (!pa || pa->type != BUFFER)
            THROW((type_id, "Packet submitted in an invalid format"));

        header->s[6] = (unsigned char) pa->u.buffer->len/256;
        header->s[7] = (unsigned char) pa->u.buffer->len%256;

        len = out->len + header->len + pa->u.buffer->len - 2;
        out = (Buffer *) erealloc(out, sizeof(Buffer) + len);
        MEMCPY(out->s + out->len, header->s, header->len);
        out->len += header->len;
        MEMCPY(out->s + out->len, pa->u.buffer->s, pa->u.buffer->len);
        out->len += pa->u.buffer->len;
    }

    RETURN_BUFFER(out);
}

