/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#define _io_

#include "defs.h"

#include <ctype.h>
#include <string.h>
#include "cdc_pcode.h"
#include "util.h"
#include "cache.h"
#include "net.h"

INTERNAL void connection_read(Conn *conn);
INTERNAL void connection_write(Conn *conn);
INTERNAL Conn *connection_add(Int fd, Long objnum);
INTERNAL void connection_discard(Conn *conn);
INTERNAL void pend_discard(pending_t *pend);
INTERNAL void server_discard(server_t *serv);

INTERNAL Conn * connections;  /* List of client connections. */
INTERNAL server_t     * servers;      /* List of server sockets. */
INTERNAL pending_t    * pendings;     /* List of pending connections. */

/*
// --------------------------------------------------------------------
// Flush defunct connections and files.
//
// Notify the connection object of any dead connections and delete them.
*/

void flush_defunct(void) {
    Conn **connp, *conn;
    server_t     **servp, *serv;
    pending_t    **pendp, *pend;

    connp = &connections;
    while (*connp) {
        conn = *connp;
        if (conn->flags.dead && conn->write_buf->len == 0) {
            *connp = conn->next;
            connection_discard(conn);
        } else {
            connp = &conn->next;
        }
    }

    servp = &servers;
    while (*servp) {
        serv = *servp;
        if (serv->dead) {
            *servp = serv->next;
            server_discard(serv);
        } else {
            servp = &serv->next;
        }
    }

    pendp = &pendings;
    while (*pendp) {
        pend = *pendp;
        if (pend->finished) {
            *pendp = pend->next;
            pend_discard(pend);
        } else {
            pendp = &pend->next;
        }
    }
}

/*
// --------------------------------------------------------------------
// Call io_event_wait() to wait for something to happen.  The return
// value is nonzero if an I/O event occurred.  If there is a new
// connection, then *fd will be set to the descriptor of the new
// connection; otherwise, it is set to -1.
*/

void handle_io_event_wait(Int seconds) {
    io_event_wait(seconds, connections, servers, pendings);
}

/*
// --------------------------------------------------------------------
*/

void handle_connection_input(void) {
    Conn * conn;

    for (conn = connections; conn; conn = conn->next) {
        if (conn->flags.readable && !conn->flags.dead)
            connection_read(conn);
    }
}

/*
// --------------------------------------------------------------------
*/
void handle_connection_output(void) {
    Conn * conn;

    for (conn = connections; conn; conn = conn->next) {
        if (conn->flags.writable)
            connection_write(conn);
    }
}

/*
// --------------------------------------------------------------------
*/
void handle_new_and_pending_connections(void) {
    Conn *conn;
    server_t *serv;
    pending_t *pend;
    cStr *str;
    cData d1, d2, d3;

    /* Look for new connections on the server sockets. */
    for (serv = servers; serv; serv = serv->next) {
        if (serv->client_socket == -1)
            continue;
        conn = connection_add(serv->client_socket, serv->objnum);
        serv->client_socket = -1;
        str = string_from_chars(serv->client_addr, strlen(serv->client_addr));
        d1.type = STRING;
        d1.u.str = str;
        d2.type = STRING;
        d2.u.str = serv->addr; /* dont dup, task() will */
        d3.type = INTEGER;
        d3.u.val = serv->client_port;
        vm_task(conn->objnum, connect_id, 3, &d1, &d2, &d3);
        string_discard(str);
    }

    /* Look for pending connections succeeding or failing. */
    for (pend = pendings; pend; pend = pend->next) {
        if (pend->finished) {
            if (pend->error == NOT_AN_IDENT) {
                conn = connection_add(pend->fd, pend->objnum);
                d1.type = INTEGER;
                d1.u.val = pend->task_id;
                vm_task(conn->objnum, connect_id, 1, &d1);
            } else {
                SOCK_CLOSE(pend->fd);
                d1.type = INTEGER;
                d1.u.val = pend->task_id;
                d2.type = T_ERROR;
                d2.u.error = pend->error;
                vm_task(pend->objnum, failed_id, 2, &d1, &d2);
            }
        }
    }
}

/*
// --------------------------------------------------------------------
// This will attempt to find a connection associated with an object.
// For faster hunting we will check obj->conn, which may be set to NULL
// even though a connection may exist (the pointer is only valid while
// the object is in the cache, and is reset to NULL when it is read from
// disk).  If obj->conn is NULL and a connection exists, we set
// obj->conn to the connection, so we will know it next time.
//
// Note: if more than one connection is associated with an object, this
// will only return the most recent connection.  Hopefully more than one
// connection will not get associated, we need to hack the server to
// blast old connections when new ones are associated, or to deny new
// ones.  Either way the db should be paying close attention to what
// is occuring.
//
// Once new connections bump old connections, this problem will go
// away.
*/

Conn * find_connection(Obj * obj) {

    /* obj->conn is only for faster lookups */
    if (obj->conn == NULL) {
        Conn * conn;

        /* lets try and find the connection */
        for (conn = connections; conn; conn = conn->next) {
            if (conn->objnum == obj->objnum && !conn->flags.dead) {
                obj->conn = conn;
                break;
            }
        }
    }

    /* it could still be NULL */
    return obj->conn;
}

/*
// --------------------------------------------------------------------
// returning the connection is what we are using as a status report, if
// there is no connection, it will be NULL, and we will know.
*/

Conn * ctell(Obj * obj, cBuf * buf) {
    Conn * conn = find_connection(obj);

    if (conn != NULL)
        conn->write_buf = buffer_append(conn->write_buf, buf);

    return conn;
}

/*
// --------------------------------------------------------------------
*/

Int boot(Obj * obj) {
    Conn * conn = find_connection(obj);

    if (conn != NULL) {
        conn->flags.dead = 1;
        return 1;
    }

    return 0;
}

/*
// --------------------------------------------------------------------
*/

Int tcp_server(Int port, char * ipaddr, Long objnum) {
    server_t * cnew;
    SOCKET server_socket;

    /* Check if a server already exists for this port and address */
    for (cnew = servers; cnew; cnew = cnew->next) {
        if (cnew->port == port) {
            if (ipaddr && strcmp(string_chars(cnew->addr), ipaddr))
                continue;
            cnew->objnum = objnum;
            cnew->dead = 0;
            return TRUE;
        }
    }

    /* Get a server socket for the port. */
    server_socket = get_tcp_socket(port, ipaddr);
    if (server_socket == SOCKET_ERROR)
        return FALSE;

    cnew = EMALLOC(server_t, 1);
    cnew->server_socket = server_socket;
    cnew->client_socket = -1;
    cnew->port = port;
    if (ipaddr)
        cnew->addr = string_from_chars(ipaddr, strlen(ipaddr));
    else
        cnew->addr = string_new(0);
    cnew->objnum = objnum;
    cnew->dead = 0;
    cnew->next = servers;
    servers = cnew;

    return TRUE;
}

Int udp_server(Int port, char * ipaddr, Long objnum) {
    SOCKET server_socket;

    /* Get a server socket for the port. */
    server_socket = get_udp_socket(port, ipaddr);
    if (server_socket == SOCKET_ERROR)
        return FALSE;

    connection_add(server_socket, objnum);
    return TRUE;
}

/*
// --------------------------------------------------------------------
*/
Int remove_server(Int port) {
    server_t **servp;

    for (servp = &servers; *servp; servp = &((*servp)->next)) {
        if ((*servp)->port == port) {
            (*servp)->dead = 1;
            return 1;
        }
    }

    return 0;
}

/*
// --------------------------------------------------------------------
*/
/* rewrote to reduce buffer copies, by reading from the socket into a
   pre-allocated static buffer that we re-use.  -Brandon */
INTERNAL void connection_read(Conn *conn) {
    Int len;
    cData d;

    /* DOH, something is still using out buffer, lets let
       it keep it and we'll get a new sandbox to play in */
    if (socket_buffer->refs > 1) {
        socket_buffer->refs--;
        socket_buffer = buffer_new(BIGBUF);
    }

    len = SOCK_READ(conn->fd, (void *) socket_buffer->s, BIGBUF);
    if (len == SOCKET_ERROR) {
        if (GETERR() == ERR_INTR)
            return;

        if (GETERR() != ERR_AGAIN) {
            /* The connection closed. */
            conn->flags.readable = 0;
            conn->flags.dead = 1;
            return;
        }
        /* hrm.. we got ERR_AGAIN, do nothing this time */
        return;
    } else if (len == 0) {
        conn->flags.readable = 0;
        conn->flags.dead = 1;
    }

    conn->flags.readable = 0;

    /* We successfully read some data.  Handle it. */
    socket_buffer->refs++;
    socket_buffer->len = len;
    d.type = BUFFER;
    d.u.buffer = socket_buffer;
    vm_task(conn->objnum, parse_id, 1, &d);
    socket_buffer->refs--;
}

/*
// --------------------------------------------------------------------
*/
INTERNAL void connection_write(Conn *conn) {
    cBuf *buf = conn->write_buf;
    Int r;

    r = SOCK_WRITE(conn->fd, buf->s, buf->len);
    conn->flags.writable = 0;

    /* We lost the connection. */
    if ((r == SOCKET_ERROR) && (GETERR() != ERR_AGAIN)) {
       conn->flags.dead = 1;
       buf = buffer_resize(buf, 0);
    } else {
       MEMMOVE(buf->s, buf->s + r, buf->len - r);
       buf = buffer_resize(buf, buf->len - r);
    }

    conn->write_buf = buf;
}

/*
// --------------------------------------------------------------------
*/
INTERNAL Conn * connection_add(Int fd, Long objnum) {
    Conn * conn;

    /* clear old connections to this objnum */
    for (conn = connections; conn; conn = conn->next) {
        if (conn->objnum == objnum && !conn->flags.dead)
            conn->flags.dead = 1;
    }

    /* initialize new connection */
    conn = EMALLOC(Conn, 1);
    conn->fd = fd;
    conn->write_buf = buffer_new(0);
    conn->objnum = objnum;
    conn->flags.readable = 0;
    conn->flags.writable = 0;
    conn->flags.dead = 0;
    conn->next = connections;
    connections = conn;

    return conn;
}

/*
// --------------------------------------------------------------------
*/
INTERNAL void connection_discard(Conn *conn) {
    Obj    * obj;
    cObjnum  objnum;

    objnum = conn->objnum;

    /* reset the conn variable on the object */
    obj = cache_retrieve(conn->objnum);
    if (obj != NULL) {
        obj->conn = NULL;
        cache_discard(obj);
    }

    /* Free the data associated with the connection. */
    SOCK_CLOSE(conn->fd);
    buffer_discard(conn->write_buf);
    efree(conn);

    /* Notify connection object that the connection is gone */
    vm_task(objnum, disconnect_id, 0);
}

/*
// --------------------------------------------------------------------
*/
INTERNAL void pend_discard(pending_t *pend) {
    efree(pend);
}

/*
// --------------------------------------------------------------------
*/
INTERNAL void server_discard(server_t *serv) {
    SOCK_CLOSE(serv->server_socket);
    string_discard(serv->addr);
    efree(serv);
}

/*
// --------------------------------------------------------------------
*/
Long make_connection(char *addr, Int port, cObjnum receiver) {
    pending_t *cnew;
    SOCKET socket;
    Long result;

    result = non_blocking_connect(addr, port, &socket);
    if (result == address_id || result == socket_id)
        return result;
    cnew = TMALLOC(pending_t, 1);
    cnew->fd = socket;
    cnew->task_id = task_id;
    cnew->objnum = receiver;
    cnew->finished = 0;
    cnew->error = result;
    cnew->next = pendings;
    pendings = cnew;
    return NOT_AN_IDENT;
}

Long make_udp_connection(char *addr, Int port, cObjnum receiver) {
    pending_t *cnew;
    SOCKET socket;
    Long result;

    result = udp_connect(addr, port, &socket);
    if (result == address_id || result == socket_id)
        return result;
    cnew = TMALLOC(pending_t, 1);
    cnew->fd = socket;
    cnew->task_id = task_id;
    cnew->objnum = receiver;
    cnew->finished = 0;
    cnew->error = result;
    cnew->next = pendings;
    pendings = cnew;
    return NOT_AN_IDENT;
}

/*
// --------------------------------------------------------------------
// Write out everything in connections' write buffers.  Called by main()
// before exiting; does not modify the write buffers to reflect writing.
*/

void flush_output(void) {
    Conn  * conn;
    unsigned char * s;
    Int len, r;

    /* do connections */
    for (conn = connections; conn; conn = conn->next) {
        s = conn->write_buf->s;
        len = conn->write_buf->len;
        while (len) {
            r = SOCK_WRITE(conn->fd, s, len);
            if ((r == SOCKET_ERROR) && (GETERR() != ERR_AGAIN))
                break;
	    /* 
	     * If it would've blocked, then don't change len or s,
             * so set the bytes written to 0
             */
            if ((r == SOCKET_ERROR) && (GETERR() == ERR_AGAIN))
                r = 0;
            len -= r;
            s += r;
        }
    }
}

#undef _io_
