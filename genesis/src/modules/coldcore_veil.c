/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/coldcore_veil.c
// ---
// Veil packets buffer manipulation module
*/

#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"
#include "memory.h"

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
void op_buf_to_veil_packets(void) {
    data_t * args;
    int      numargs;
    Buffer * buf;
    list_t * packets;

    if (!func_init_1_or_2(&args, &numargs, BUFFER, BUFFER))
        return;

    /* if they sent us a second argument, concatenate it on the end */
    buf = buffer_dup(args[0].u.buffer);
    if (numargs == 2)
        buffer_append(buf, args[1].u.buffer);
    packets = buffer_to_veil_packets(buf);
    buffer_discard(buf);

    pop(numargs);
    push_list(packets);
    list_discard(packets);
}

/*
// -------------------------------------------------------------------
// Convert to a buffer from the standard 'VEIL packet' format
//
// in leiu of speed, we do not do COMPLETE checking on every argument
// sent to the packet, just make sure you do it right
*/
void op_buf_from_veil_packets(void) {
    data_t        * d,
                  * pa,
                  * args;
    Buffer        * out,
                  * header;
    list_t        * p, * packets;
    int             len;

    if (!func_init_1(&args, LIST))
        return;

    header = buffer_new(HEADER_SIZE);
    header->s[1] = (unsigned char) 0;
    header->s[4] = (unsigned char) 0;
    header->s[5] = (unsigned char) 0;

    out = buffer_new(0);
    packets = args[0].u.list;

    for (d = list_first(packets); d; d = list_next(packets, d)) {
        if (d->type != LIST) {
            cthrow(type_id, "Packets submitted in an invalid format");
            return;
        }
        p = d->u.list;
        pa = list_first(p);
        if (!pa || pa->type != INTEGER) {
            cthrow(type_id, "Packet submitted in an invalid format");
            return;
        }

        header->s[0] = (unsigned char) 0;
        if (pa->u.val)
            header->s[0] |= VEIL_P_PUSH;

        pa = list_next(p, pa);
        if (!pa) {
            cthrow(type_id, "Not enough elements in the packet!");
            return;
        }
        if (pa->type == SYMBOL) {
            if (pa->u.symbol == pabort_id)
                header->s[0] |= VEIL_P_ABORT;
            else if (pa->u.symbol == pclose_id)
                header->s[0] |= VEIL_P_CLOSE;
            else if (pa->u.symbol == popen_id)
                header->s[0] |= VEIL_P_OPEN;
        }

        pa = list_next(p, pa);
        if (!pa || pa->type != INTEGER) {
            cthrow(type_id, "Packet submitted in an invalid format");
            return;
        }

        header->s[2] = (unsigned char) pa->u.val/256;
        header->s[3] = (unsigned char) pa->u.val%256;

        pa = list_next(p, pa);
        if (!pa || pa->type != BUFFER) {
            cthrow(type_id, "Packet submitted in an invalid format");
            return;
        }

        header->s[6] = (unsigned char) pa->u.buffer->len/256;
        header->s[7] = (unsigned char) pa->u.buffer->len%256;

        len = out->len + header->len + pa->u.buffer->len - 2;
        out = (Buffer *) erealloc(out, sizeof(Buffer) + len);
        MEMCPY(out->s + out->len, header->s, header->len);
        out->len += header->len;
        MEMCPY(out->s + out->len, pa->u.buffer->s, pa->u.buffer->len);
        out->len += pa->u.buffer->len;
    }

    pop(1);
    push_buffer(out);
    buffer_discard(out);
}

