/*
// Full copyright information is available in the file ../doc/CREDITS
//
// RFC references: inverse name resolution--1293, 903 1035 - domain name system
*/

#define _BSD 44 /* For RS6000s. */
#define _NET_C_
#include "defs.h"

#include <sys/types.h>
#ifdef __UNIX__
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <ctype.h>
#include <fcntl.h>
#include "net.h"
#include "util.h"

INTERNAL SOCKET grab_port(Int port, char * addr, int socktype);
static Long translate_connect_error(Int error);

static struct sockaddr_in sockin;	/* An internet address. */
static int addr_size = sizeof(sockin);	/* Size of sockin. */

Long server_failure_reason;

void init_net(void) {
#ifdef __Win32__
    WSADATA wsa;

    WSAStartup(0x0101, &wsa);
#endif
    socket_buffer = buffer_new(BIGBUF);
}

void uninit_net(void) {
#ifdef __Win32__
    WSACleanup();
#endif
    buffer_discard(socket_buffer);
}

/*
// -------------------------------------------------------------------
// inet_aton() courtesy of Luc Girardin <girardin@hei.unige.ch>, I dont
// know where he got it  8)
*/

#ifndef HAVE_INET_ATON
int inet_aton (const char * cp, struct in_addr * addr) {
    unsigned long parts[4];
    register unsigned long val;
    register unsigned long part0;
    register unsigned long part1;
    register unsigned long part2;
    register unsigned long part3;
    int part;
    char *next;

    part = 0;

    forever {
        if (!isdigit (*cp)) /* not decimal digit or leading 0, 0x */
            return 0;

        errno = 0;
        parts[part++] = strtoul (cp, &next, 0); /* leading 0=octal, 0x=hex */
        if (errno == ERANGE)
            return 0;
      
        if (*next == '.') {
            if (part >= 4)
                return 0;

            cp = next + 1;
        } else
            break; /* from for loop */
    }
    /* Check for trailing non-whitespace characters */
    if (strlen (next) != strspn (next, " \t\n\v\f\r"))
        return 0;

    /* Concoct the address according to the number of parts specified. */

    val = 0;
    part0 = parts[0];
    part1 = parts[1];
    part2 = parts[2];
    part3 = parts[3];

    switch (part) {
      case 4: /* a.b.c.d -- 8.8.8.8 bits */
          if (part3 > 0xff || part2 > 0xff)
              return 0;
          val = part3;
          part2 <<= 8;
          /* FALLTHROUGH */
      case 3: /* a.b.c -- 8.8.16 bits */
          if (part2 > 0xffff || part1 > 0xff)
              return 0;
          val |= part2;
          part1 <<= 16;
          /* FALLTHROUGH */
      case 2: /* a.b -- 8.24 bits */
          if (part1 > 0xffffff || part0 > 0xff)
              return 0;
          val |= part1;
          part0 <<= 24;
          /* FALLTHROUGH */
      case 1: /* a -- 32 bits */
          val |= part0;
    }

    addr->s_addr = htonl (val);
    return 1;
}
#endif

/*
// -----------------------------------------------------------------------
// prebind things--basically call socket() and bind() but nothing else,
// later we can call other things ..
*/

typedef struct Prebind Prebind;
struct Prebind {
    SOCKET    sock;
    uShort    port;
    Bool      tcp;
    char      addr[BUF];
    Prebind * next;
};
Prebind * prebound = NULL;

#define DIE(_reason_) { \
        fputs(_reason_, stderr); \
        exit(1); \
    }

Bool prebind_port(int port, char * addr, int tcp) {
    SOCKET    sock;
    Prebind * pb;

    /* address too long? */
    if (addr && (strlen(addr) > BUF))
        return NO;

    sock = grab_port(port, addr, tcp ? SOCK_STREAM : SOCK_DGRAM);
    if (sock != SOCKET_ERROR) {
        pb = (Prebind *) malloc(sizeof(Prebind));
        pb->sock = sock;
        pb->port = port;
        pb->tcp = tcp;
        if (addr)
            strcpy(pb->addr, addr);
        else
            pb->addr[0] = (char) NULL;
        pb->next = prebound;
        prebound = pb;
    } else if (server_failure_reason == address_id) {
        fprintf(stderr, "** Invalid internet address: '%s'\n", addr);
        exit(1);
    } else if (server_failure_reason == socket_id) {
        fprintf(stderr, "** Unable to open socket: %s\n", strerror(errno));
        exit(1);
    } else if (server_failure_reason == bind_id) {
        fprintf(stderr, "** Unable to bind port: %s\n", strerror(errno));
        exit(1);
    }

    return YES;
}

INTERNAL int use_prebound(SOCKET * sock, int port, char * addr, int socktype) {
    Prebind  * pb,
            ** pbp = &prebound;

    while (*pbp) { 
        pb = *pbp;
        if (pb->port == port) {
            if (addr) {
                if (!pb->addr[0] || strccmp(pb->addr, addr)) {
                    server_failure_reason = preaddr_id;
                    return F_FAILURE;
                }
            } else if (pb->addr[0]) {
                server_failure_reason = preaddr_id;
                return F_FAILURE;
            }
            if ((pb->tcp && socktype == SOCK_DGRAM) ||
                (!pb->tcp && socktype == SOCK_STREAM))
            {
                server_failure_reason = pretype_id;
                return F_FAILURE;
            }
            *sock = pb->sock;
            *pbp = pb->next;
            free(pb);
            return TRUE;
        } else {
            pbp = &pb->next;
        }
    }

    return FALSE;
}

INTERNAL SOCKET grab_port(Int port, char * addr, int socktype) {
    int    one = 1;
    Int flags;
    SOCKET sock;

    /* see if its pre-bound? */
    switch (use_prebound(&sock, port, addr, socktype)) {
        case F_FAILURE:
            return SOCKET_ERROR;
        case TRUE:
            return sock;
    }

    /* verify the address first */
    memset(&sockin, 0, sizeof(sockin));               /* zero it */
    sockin.sin_family = AF_INET;                      /* set inet */
    sockin.sin_port = htons((unsigned short) port);   /* set port */

    if (addr && !inet_aton(addr, &sockin.sin_addr)) {
        server_failure_reason = address_id;
        return SOCKET_ERROR;
    }

    /* Create a socket. */
    sock = socket(AF_INET, socktype, 0);
    if (sock == SOCKET_ERROR) {
	server_failure_reason = socket_id;
	return SOCKET_ERROR;
    }

    /* Set SO_REUSEADDR option to avoid restart problems. */
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(Int));

    /* Bind the socket to port. */
    if (bind(sock, (struct sockaddr *) &sockin, sizeof(sockin)) == F_FAILURE) {
	server_failure_reason = bind_id;
	return SOCKET_ERROR;
    }

#ifdef __Win32__
    one = 1;
    ioctlsocket(sock, FIONBIO, &one);
#else
    flags = fcntl(sock, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sock, F_SETFL, flags);
#endif

    return sock;
}

SOCKET get_tcp_socket(Int port, char * addr) {
    SOCKET sock;
    
    sock = grab_port(port, addr, SOCK_STREAM);

    if (sock == SOCKET_ERROR)
        return SOCKET_ERROR;

    listen(sock, 8);

    return sock;
}

SOCKET get_udp_socket(Int port, char * addr) {
    SOCKET sock;
    
    sock = grab_port(port, addr, SOCK_DGRAM);

    if (sock == SOCKET_ERROR)
        return SOCKET_ERROR;

    return sock;
}

/* Wait for I/O events.  sec is the number of seconds we can wait before
 * returning, or -1 if we can wait forever.  Returns nonzero if an I/O event
 * happened. */
Int io_event_wait(Int sec, Conn *connections, server_t *servers,
		  pending_t *pendings)
{
    struct timeval tv, *tvp;
    Conn *conn;
    server_t *serv;
    pending_t *pend;
    fd_set read_fds, write_fds, except_fds;
    Int flags, nfds, count, result, error;
    int dummy = sizeof(int);

    /* Set time structure according to sec. */
    if (sec == -1) {
	tvp = NULL;
        /* this is a rather odd thing to happen for me */
        write_err("select: forever wait");
    } else {
	tv.tv_sec = (long) sec;
	tv.tv_usec = 0;
	tvp = &tv;
    }

    /* Begin with blank file descriptor masks and an nfds of 0. */
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    nfds = 0;

    /* Listen for new data on connections, and also check for ability to write
     * to them if we have data to write. */
    for (conn = connections; conn; conn = conn->next) {
	if (!conn->flags.dead) {
            FD_SET(conn->fd, &except_fds);
	    FD_SET(conn->fd, &read_fds);
        }
	if (conn->write_buf->len)
	    FD_SET(conn->fd, &write_fds);
	if (conn->fd >= nfds)
	    nfds = conn->fd + 1;
    }

    /* Listen for connections on the server sockets. */
    for (serv = servers; serv; serv = serv->next) {
	FD_SET(serv->server_socket, &read_fds);
	if (serv->server_socket >= nfds)
	    nfds = serv->server_socket + 1;
    }

    /* Check pending connections for ability to write. */
    for (pend = pendings; pend; pend = pend->next) {
	if (pend->error != NOT_AN_IDENT) {
	    /* The connect has already failed; just set the finished bit. */
	    pend->finished = 1;
	} else {
	    FD_SET(pend->fd, &write_fds);
	    if (pend->fd >= nfds)
		nfds = pend->fd + 1;
	}
    }

#ifdef __Win32__
    /* Winsock 2.0 will return EINVAL (invalid argument) if there are no 
       sockets checked in any of the FDSETs.  At least one server must be
       listening before the call is made.  Winsock 1.1 behaves differently.
     */

    if (servers || connections || pendings) {
#endif
    /* Call select(). */
    count = select(nfds, &read_fds, &write_fds, &except_fds, tvp);
#ifdef __Win32__
    } else {
        count = 0;
    }
#endif

    /* Lose horribly if select() fails on anything but an interrupted system
     * call.  On ERR_INTR, we'll return 0. */
    if (count == SOCKET_ERROR) {
        if (GETERR() != ERR_INTR)
            panic("select() failed");

        /* Stop and return zero if no I/O events occurred. */
        return 0;
    }

    /* Check if any connections are readable or writable. */
    for (conn = connections; conn; conn = conn->next) {
	if (FD_ISSET(conn->fd, &except_fds)) {
            conn->flags.dead = 1;
            fprintf(stderr, "An exception occurred during select()\n");
        }
	if (FD_ISSET(conn->fd, &read_fds))
	    conn->flags.readable = 1;
	if (FD_ISSET(conn->fd, &write_fds))
	    conn->flags.writable = 1;
    }

    /* Check if any server sockets have new connections. */
    for (serv = servers; serv; serv = serv->next) {
	if (FD_ISSET(serv->server_socket, &read_fds)) {
	    serv->client_socket = accept(serv->server_socket,
				 (struct sockaddr *) &sockin, &addr_size);
	    if (serv->client_socket == SOCKET_ERROR)
		continue;
#ifdef __Win32__
            result = 1;
            ioctlsocket(serv->client_socket, FIONBIO, &result);
#else
            flags = fcntl(serv->client_socket, F_GETFL);
	    flags |= O_NONBLOCK;
	    fcntl(serv->client_socket, F_SETFL, flags);
#endif

	    /* Get address and local port of client. */
	    strcpy(serv->client_addr, inet_ntoa(sockin.sin_addr));
	    serv->client_port = ntohs(sockin.sin_port);

	    /* Set the CLOEXEC flag on socket so that it will be closed for a
	     * execute() operation. */
#ifdef FD_CLOEXEC
	    flags = fcntl(serv->client_socket, F_GETFD);
	    flags |= FD_CLOEXEC;
	    fcntl(serv->client_socket, F_SETFD, flags);
#endif
	}
    }

    /* Check if any pending connections have succeeded or failed. */
    for (pend = pendings; pend; pend = pend->next) {
	if (FD_ISSET(pend->fd, &write_fds)) {
	    result = getpeername(pend->fd, (struct sockaddr *) &sockin,
				 &addr_size);
	    if (result == SOCKET_ERROR) {
		getsockopt(pend->fd, SOL_SOCKET, SO_ERROR, (char *) &error,
			   &dummy);
		pend->error = translate_connect_error(error);
	    } else {
		pend->error = NOT_AN_IDENT;
	    }
	    pend->finished = 1;
	}
    }

    /* Return nonzero, indicating that at least one I/O event occurred. */
    return 1;
}

Long non_blocking_connect(char *addr, Int port, Int *socket_return)
{
    SOCKET fd;
    Int    result, flags;
    struct in_addr inaddr;
    struct sockaddr_in saddr;

    /* Convert address to struct in_addr. */
    inaddr.s_addr = inet_addr(addr);
    if (inaddr.s_addr == -1)
	return address_id;

    /* Get a socket for the connection. */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == SOCKET_ERROR)
	return socket_id;

    /* Set the socket non-blocking. */
#ifdef __Win32__
    result = 1;
    ioctlsocket(fd, FIONBIO, &result);
#else
    flags = fcntl(fd, F_GETFL);
#ifdef FNDELAY
    flags |= FNDELAY;
#else
#ifdef O_NDELAY
    flags |= O_NDELAY;
#endif
#endif
    fcntl(fd, F_SETFL, flags);
#endif

    /* Make the connection. */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((unsigned short) port);
    saddr.sin_addr = inaddr;
    do {
	result = connect(fd, (struct sockaddr *) &saddr, sizeof(saddr));
    } while (result == SOCKET_ERROR && GETERR() == ERR_INTR);

    *socket_return = fd;
    if (result != SOCKET_ERROR || GETERR() == ERR_INPROGRESS || GETERR() == ERR_AGAIN)
	return NOT_AN_IDENT;
    else
	return translate_connect_error(GETERR());
}

Long udp_connect(char *addr, Int port, Int *socket_return)
{
    SOCKET fd;
    Int    result, flags;
    struct in_addr inaddr;
    struct sockaddr_in saddr;

    /* Convert address to struct in_addr. */
    inaddr.s_addr = inet_addr(addr);
    if (inaddr.s_addr == -1)
	return address_id;

    /* Get a socket for the connection. */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == SOCKET_ERROR)
	return socket_id;

    /* Set the socket non-blocking. */
#ifdef __Win32__
    result = 1;
    ioctlsocket(fd, FIONBIO, &result);
#else
    flags = fcntl(fd, F_GETFL);
#ifdef FNDELAY
    flags |= FNDELAY;
#else
#ifdef O_NDELAY
    flags |= O_NDELAY;
#endif
#endif
    fcntl(fd, F_SETFL, flags);
#endif

    /* Make the connection. */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((unsigned short) port);
    saddr.sin_addr = inaddr;
    do {
	result = connect(fd, (struct sockaddr *) &saddr, sizeof(saddr));
    } while (result == SOCKET_ERROR && GETERR() == ERR_INTR);

    *socket_return = fd;
    if (result != SOCKET_ERROR || GETERR() == ERR_INPROGRESS || GETERR() == ERR_AGAIN)
	return NOT_AN_IDENT;
    else
	return translate_connect_error(GETERR());
}

static Long translate_connect_error(Int error)
{
    switch (error) {

      case ERR_CONNREFUSED:
	return refused_id;

      case ERR_NETUNREACH:
	return net_id;

      case ERR_TIMEDOUT:
	return timeout_id;

      default:
	return other_id;
    }
}
