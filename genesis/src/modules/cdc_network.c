/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_network.c
// ---
// Network Module
*/

/*
// NOTES: echo() -> connection_write() ?
*/

#include "config.h"
#include "defs.h"
#include "y.tab.h"
#include "cdc_types.h"
#include "execute.h"
#include "net.h"

/*
// -----------------------------------------------------------------
//
// Modifies: cur_player, contents of cur_conn.
// Effects: If called by the system object with a dbref argument,
//          assigns that dbref to cur_conn->dbref and to cur_player
//          and returns 1, unless there is no current connection, in
//          which case it returns 0.
//
*/

void op_reassign_connection(void) {
    data_t * args;

    /* Accept a dbref. */
    if (!func_init_1(&args, DBREF))
        return;

    if (cur_conn) {
        cur_conn->dbref = args[0].u.dbref;
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
void op_bind_port(void) {
    data_t * args;

    /* Accept a port to bind to, and a dbref to handle connections. */
    if (!func_init_2(&args, INTEGER, DBREF))
        return;

    if (add_server(args[0].u.val, args[1].u.dbref))
        push_int(1);
    else if (server_failure_reason == socket_id)
        cthrow(socket_id, "Couldn't create server socket.");
    else /* (server_failure_reason == bind_id) */
        cthrow(bind_id, "Couldn't bind to port %d.", args[0].u.val);
}

/*
// -----------------------------------------------------------------
*/
void op_unbind_port(void) {
    data_t * args;

    /* Accept a port number. */
    if (!func_init_1(&args, INTEGER))
        return;

    if (!remove_server(args[0].u.val))
        cthrow(servnf_id, "No server socket on port %d.", args[0].u.val);
    else
        push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void op_open_connection(void) {
    data_t *args;
    char *address;
    int port;
    Dbref receiver;
    long r;

    if (!func_init_3(&args, STRING, INTEGER, DBREF))
        return;

    address = string_chars(args[0].u.str);
    port = args[1].u.val;
    receiver = args[2].u.dbref;

    r = make_connection(address, port, receiver);
    if (r == address_id)
        cthrow(address_id, "Invalid address");
    else if (r == socket_id)
        cthrow(socket_id, "Couldn't create socket for connection");
    pop(3);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void op_hostname(void) {
    data_t *args;
    string_t *r;

    /* Accept a port number. */
    if (!func_init_1(&args, STRING))
        return;

    r = hostname(args[0].u.str->s);

    pop(1);
    push_string(r);
}

/*
// -----------------------------------------------------------------
*/
void op_ip(void) {
    data_t *args;
    string_t *r;

    /* Accept a hostname. */
    if (!func_init_1(&args, STRING))
        return;

    r = ip(args[0].u.str->s);

    pop(1);
    push_string(r);
}

void op_close_connection(void) {
    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* Kick off anyone assigned to the current object. */
    push_int(boot(cur_frame->object));
}

/*
// Echo a buffer to the connection
*/
void op_echo(void)
{
    data_t *args;

    /* Accept a string to echo. */
    if (!func_init_1(&args, BUFFER))
        return;

    /* Write the string to any connection associated with this object.  */
    tell(cur_frame->object, args[0].u.buffer);

    pop(1);
    push_int(1);
}

