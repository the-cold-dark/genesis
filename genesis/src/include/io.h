/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_io_h
#define cdc_io_h

typedef struct Conn Conn;
typedef struct server_s     server_t;
typedef struct pending_s    pending_t;

#include "net.h"

struct Conn {
    SOCKET fd;                /* File descriptor for input and output. */
    cBuf * write_buf;     /* Buffer for network output. */
    cObjnum    objnum;       /* Object connection is associated with. */
    struct {
        char readable;        /* Connection has new data pending. */
        char writable;        /* Connection can be written to. */
        char dead;            /* Connection is defunct. */
    } flags;
    Conn * next;
};

struct server_s {
    SOCKET         server_socket;
    unsigned short port;
    cStr         * addr;
    cObjnum        objnum;
    Int            dead;
    SOCKET         client_socket;
    char           client_addr[20];
    unsigned short client_port;
    server_t     * next;
};

struct pending_s {
    SOCKET fd;
    Long task_id;
    cObjnum objnum;
    Long error;
    Int finished;
    pending_t *next;
};

void flush_defunct(void);
void handle_new_and_pending_connections(void);
void handle_io_event_wait(Int seconds);
void handle_connection_input(void);
void handle_connection_output(void);
Conn * find_connection(Obj * obj);
Conn * ctell(Obj * obj, cBuf *buf);
Int  boot(Obj * obj);
Int  tcp_server(Int port, char * addr, Long objnum);
Int  udp_server(Int port, char * addr, Long objnum);
Int  remove_server(Int port);
Long make_connection(char *addr, Int port, cObjnum receiver);
Long make_udp_connection(char *addr, Int port, cObjnum receiver);
void flush_output(void);
Long udp_connect(char *addr, Int port, Int *socket_return);

#endif

