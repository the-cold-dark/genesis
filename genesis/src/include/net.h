/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_net_h
#define cdc_net_h

#ifdef __Win32__

#define SOCK_CLOSE(_sc_)                  closesocket(_sc_)
#define SOCK_READ(_sc_, _buf_, _len_)     recv(_sc_, _buf_, _len_, 0)
#define SOCK_WRITE(_sc_, _buf_, _len_)    send(_sc_, _buf_, _len_, 0)

#else

typedef Int SOCKET;

#define SOCK_CLOSE(_sc_)                  close(_sc_)
#define SOCK_READ(_sc_, _buf_, _len_)     read(_sc_, _buf_, _len_)
#define SOCK_WRITE(_sc_, _buf_, _len_)    write(_sc_, _buf_, _len_)

#endif

Int io_event_wait(Int sec, Conn *connections, server_t *servers,
		  pending_t *pendings);
Long non_blocking_connect(char *addr, Int port, Int *socket_return);
void init_net(void);
void uninit_net(void);

SOCKET get_tcp_socket(Int port, char * addr);
SOCKET get_udp_socket(Int port, char * addr);

Bool prebind_port(int port, char * addr, int tcp);

#ifdef _NET_C_
cBuf * socket_buffer;
#else
extern cBuf * socket_buffer;
#endif
extern Long server_failure_reason;

#endif

