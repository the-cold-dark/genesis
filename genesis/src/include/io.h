/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/io.h
// ---
// Declarations for input/output management.
*/

#ifndef _io_h_
#define _io_h_

typedef struct connection_s connection_t;
typedef struct server_s     server_t;
typedef struct pending_s    pending_t;

#include "cdc_types.h"

struct connection_s {
    int fd;                   /* File descriptor for input and output. */
    Buffer * write_buf;       /* Buffer for network output. */
    Dbref    dbref;           /* Object connection is associated with. */
    struct {
        char readable;        /* Connection has new data pending. */
        char writable;        /* Connection can be written to. */
        char dead;            /* Connection is defunct. */
    } flags;
    connection_t * next;
};

struct server_s {
    int server_socket;
    unsigned short port;
    Dbref dbref;
    int dead;
    int client_socket;
    char client_addr[20];
    unsigned short client_port;
    server_t *next;
};

struct pending_s {
    int fd;
    long task_id;
    Dbref dbref;
    long error;
    int finished;
    pending_t *next;
};

void flush_defunct(void);
void handle_new_and_pending_connections(void);
void handle_io_event_wait(int seconds);
void handle_connection_input(void);
void handle_connection_output(void);
connection_t * tell(object_t * obj, Buffer *buf);
int  boot(object_t * obj);
int  add_server(int port, long dbref);
int  remove_server(int port);
long make_connection(char *addr, int port, Dbref receiver);
void flush_output(void);

#endif

