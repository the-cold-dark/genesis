/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <string.h>
#include "execute.h"
#include "net.h"
#include "util.h"
#include "cache.h"

/*
// -----------------------------------------------------------------
//
// If the current object has a connection, it will reassign that
// connection too the specified object.
//
*/

void func_reassign_connection(void) {
    cData       * args;
    Conn * c;
    Obj     * obj;

    /* Accept a objnum. */
    if (!func_init_1(&args, OBJNUM))
        return;

    c = find_connection(cur_frame->object);
    if (c) {
        obj = cache_retrieve(args[0].u.objnum);
        if (!obj)
            THROW((objnf_id, "Object #%l does not exist.", args[0].u.objnum))
        else if (find_connection(obj)) {
            cthrow(perm_id, "Object %O already has a connection.", obj->objnum);
            cache_discard(obj);
            return;
        }
        c->objnum = obj->objnum;
        cache_discard(obj);
        cur_frame->object->conn = NULL;
        pop(1);
        push_int(1);
    } else {
        pop(1);
        push_int(0);
    }
}

/*
// -----------------------------------------------------------------
*/
void func_bind_port(void) {
    cData * args;
    Int     argc;
    Int     port;
    char  * addr;

    /* Accept a port to bind to, and a objnum to handle connections. */
    if (!func_init_1_or_2(&args, &argc, INTEGER, STRING))
        return;

    addr = (argc==2 ? string_chars(STR2) : (char *) NULL);
    port=INT1;

    if (((port >= 0) ? tcp_server(port, addr, cur_frame->object->objnum)
	             : udp_server(-port, addr, cur_frame->object->objnum))) {
        pop(argc);
	push_int(1);
    } else if (server_failure_reason == address_id)
        THROW((address_id, "Invalid bind address: %s", addr))
    else if (server_failure_reason == socket_id)
        THROW((socket_id, "Couldn't create server socket."))
    else if (server_failure_reason == preaddr_id)
        THROW((preaddr_id, 
               "Couldn't bind to port %d: prebound address conflict", INT1))
    else if (server_failure_reason == pretype_id) {
        if (port > 0)
            THROW((pretype_id, 
               "Couldn't bind to TCP port %d: already prebound as UDP", INT1))
        else
            THROW((pretype_id, 
               "Couldn't bind to UDP port %d: already prebound as TCP", INT1))
    } else if (addr)
        THROW((server_failure_reason,
               "Couldn't bind to port %d on address %s", INT1, addr))
    else
        THROW((server_failure_reason, "Couldn't bind to port %d.", INT1))
}

/*
// -----------------------------------------------------------------
*/
void func_unbind_port(void) {
    cData * args;

    /* Accept a port number. */
    if (!func_init_1(&args, INTEGER))
        return;

    if (!remove_server(args[0].u.val))
        THROW((servnf_id, "No server socket on port %d.", args[0].u.val))
    else {
        pop(1);
        push_int(1);
    }
}

/*
// -----------------------------------------------------------------
*/
void func_open_connection(void) {
    cData *args;
    char *address;
    Int port, argc;
    Long r;

    if (!func_init_2_or_3(&args, &argc, STRING, INTEGER, INTEGER))
        return;

    address = string_chars(args[0].u.str);
    port = args[1].u.val;

    if (argc==2)
	r = make_connection(address, port, cur_frame->object->objnum);
    else
	r = make_udp_connection(address, port, cur_frame->object->objnum);
    if (r == address_id)
        THROW((address_id, "Invalid address"))
    else if (r == socket_id)
        THROW((socket_id, "Couldn't create socket for connection"))
    pop(argc);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_close_connection(void) {
    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* Kick off anyone assigned to the current object. */
    push_int(boot(cur_frame->object));
}

/*
// -----------------------------------------------------------------
// Echo a buffer to the connection
*/
void func_cwrite(void) {
    cData *args;
    int rval;

    /* Accept a buffer to write. */
    if (!func_init_1(&args, BUFFER))
        return;

    /* Write the string to any connection associated with this object.  */
    rval = ctell(cur_frame->object, args[0].u.buffer) ? 1 : 0;

    pop(1);
    push_int(rval);
}

/*
// -----------------------------------------------------------------
// write a file to the connection
*/
void func_cwritef(void) {
    size_t        block, r;
    cData      * args;
    FILE        * fp;
    cBuf    * buf;
    cStr    * str;
    struct stat   statbuf;
    Int           nargs;

    /* Accept the name of a file to echo */
    if (!func_init_1_or_2(&args, &nargs, STRING, INTEGER))
        return;

    /* Initialize the file */
    str = build_path(args[0].u.str->s, &statbuf, DISALLOW_DIR);
    if (str == NULL)
        return;

    /* Open the file for reading. */
    fp = open_scratch_file(str->s, "rb");
    if (!fp)
        THROW((file_id, "Cannot open file \"%s\" for reading.", str->s))

    /* how big of a chunk do we read at a time? */
    if (nargs == 2) {
        if (args[1].u.val == -1)
            block = statbuf.st_size;
        else
            block = (size_t) args[1].u.val;
    } else
        block = (size_t) DEF_BLOCKSIZE;

    /* Allocate a buffer to hold the block */
    buf = buffer_new(block);

    while (!feof(fp)) {
        r = fread(buf->s, sizeof(unsigned char), block, fp);
        if (r != block) {
            if (!feof(fp)) {
                buffer_discard(buf);
                close_scratch_file(fp);
                cthrow(file_id, "Trouble reading file \"%s\": %s",
                       str->s, strerror(GETERR()));
                return;
            } else {
                buf->len = r;
                ctell(cur_frame->object, buf);
            }
        } else
            ctell(cur_frame->object, buf);
    }

    /* Discard the buffer and close the file. */
    buffer_discard(buf);
    close_scratch_file(fp);

    pop(nargs);
    push_int((cNum) statbuf.st_size);
}

/*
// -----------------------------------------------------------------
// return random info on the connection
*/
void func_connection(void) {
    cList       * info;
    cData       * list;
    Conn * c;

    if (!func_init_0())
        return;

    c = find_connection(cur_frame->object);
    if (!c)
        THROW((net_id, "No connection established."))

    info = list_new(4);
    list = list_empty_spaces(info, 4);

    list[0].type = INTEGER;
    list[0].u.val = (cNum) (c->flags.readable ? 1 : 0);
    list[1].type = INTEGER;
    list[1].u.val = (cNum) (c->flags.writable ? 1 : 0);
    list[2].type = INTEGER;
    list[2].u.val = (cNum) (c->flags.dead ? 1 : 0);
    list[3].type = INTEGER;
    list[3].u.val = (cNum) (c->fd);

    push_list(info);
    list_discard(info);
}
