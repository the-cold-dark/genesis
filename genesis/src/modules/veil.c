/*
// Full copyright information is available in the file ../doc/CREDITS
//
// VEIL packets buffer manipulation module
//
// look at http://www.cold.org/VEIL/ for more information.
//
*/

#define NATIVE_MODULE "$buffer"

#define VEIL_C

#include "veil.h"

module_t veil_module = {YES, init_veil, YES, uninit_veil};

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
// 2 bytes channel id
// 2 bytes length
*/

#define HEADER_SIZE    5

void init_veil(Int argc, char ** argv) {
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
//       [1|0, 'abort|'close|'open|0, id, `[]]
//
// The first argument represents the push bit, and the end of a message.
// id is an integer.
*/

/*
// -------------------------------------------------------------------
// internal function for buffer -> VEIL packet
*/
cList * buf_to_veil_packets(cBuf * buf) {
    Int             flags,
                    channel,
                    length,
                    blen;
    cBuf          * databuf,
                  * incomplete;
    cData           d,
                  * list;
    cList         * output,
                  * packet;
    unsigned char * cbuf;

    output = list_new(0);
    blen = buf->len;
    cbuf = buf->s;

    forever {
        /* 5 bytes will give us the header, we check length again after
           that, because the header gives us the data length */
        if (blen < HEADER_SIZE)
            break;

        /* grab the bit flags */
        flags = (Int) cbuf[0];

        /* get the Channel ID and data length */
        channel = (Int) (((int) cbuf[1] * 256) + ((int) cbuf[2]));
        length = (Int) (((int) cbuf[3] * 256) + ((int) cbuf[4]));

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
            list[1].u.symbol = ident_dup(pabort_id);
        } else if (flags & VEIL_P_CLOSE) {
            list[1].type     = SYMBOL;
            list[1].u.symbol = ident_dup(pclose_id);
        } else if (flags & VEIL_P_OPEN) {
            list[1].type     = SYMBOL;
            list[1].u.symbol = ident_dup(popen_id);
        } else {
            list[1].type  = INTEGER;
            list[1].u.val = 0;
        }

        list[2].type = INTEGER;
        list[2].u.val = channel;

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
    cBuf * buf;
    cList * packets;

    INIT_1_OR_2_ARGS(BUFFER, BUFFER);

    if (argc == 1) {
        buf = buffer_dup(_BUF(ARG1));
    } else {
        /* if they sent us a second argument, concatenate it on the end */
        buf = buffer_append(buffer_dup(_BUF(ARG1)), _BUF(ARG2));
    }

    packets = buf_to_veil_packets(buf);

    buffer_discard(buf);

    CLEAN_RETURN_LIST(packets);
}

/*
// -------------------------------------------------------------------
// Convert to a buffer from the standard 'VEIL packet' format
//
// in leiu of speed, we do not do COMPLETE checking on every argument
// sent to the packet, just make sure you do it right
*/
NATIVE_METHOD(from_veil_pkts) {
    cData       * d,
                * pa;
    cBuf        * out,
                * header;
    cList       * p,
                * packets;
    Int           len;

    INIT_1_ARG(LIST);

    header = buffer_new(HEADER_SIZE);

    out = buffer_new(0);
    packets = LIST1;

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

        header->s[1] = (unsigned char) pa->u.val/256;
        header->s[2] = (unsigned char) pa->u.val%256;

        pa = list_next(p, pa);
        if (!pa || pa->type != BUFFER)
            THROW((type_id, "Packet submitted in an invalid format"));

        header->s[3] = (unsigned char) pa->u.buffer->len/256;
        header->s[4] = (unsigned char) pa->u.buffer->len%256;

        len = out->len + header->len + pa->u.buffer->len - 2;
        out = (cBuf *) erealloc(out, sizeof(cBuf) + len);
        MEMCPY(out->s + out->len, header->s, header->len);
        out->len += header->len;
        MEMCPY(out->s + out->len, pa->u.buffer->s, pa->u.buffer->len);
        out->len += pa->u.buffer->len;
    }

    CLEAN_RETURN_BUFFER(out);
}

