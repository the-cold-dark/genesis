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

SOCKET get_server_socket(Int port, char * addr);
Int io_event_wait(Int sec, Conn *connections, server_t *servers,
		  pending_t *pendings);
Long non_blocking_connect(char *addr, Int port, Int *socket_return);
cStr * hostname(char *addr);
cStr * ip(char *addr);
void init_net(void);
void uninit_net(void);

extern Long server_failure_reason;

#endif
