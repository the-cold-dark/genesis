/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/net.h
// ---
// Declarations for network routines.
*/

#ifndef _net_h_
#define _net_h_

#include "io.h"

int get_server_socket(int port);
int io_event_wait(long sec, connection_t *connections, server_t *servers,
		  pending_t *pendings);
long non_blocking_connect(char *addr, int port, int *socket_return);
string_t *hostname(char *addr);
string_t *ip(char *addr);

extern long server_failure_reason;

#endif

