/*
// Full copyright information is available in the file ../doc/CREDITS
//
// RFC references: inverse name resolution--1293, 903 1035 - domain name system
//
// This is a quickie--full implementation can come later
//
//  -Brandon
*/

#define _BSD 44 /* For RS6000s. */

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
#include <signal.h>
#endif
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>
#include "net.h"
#include "util.h"
#include "dns.h"

/*
// -------------------------------------------------------------------------
// generic defines
// -------------------------------------------------------------------------
*/

/*#define LINE 255*/
#define NET_PORT 1153
#define MALLOC(type, num) ((type *) malloc((num) * sizeof(type)))
#define FREE(ptr)         free(ptr)
#define CONN_QUEUE 8     /* how many conns to backlog before the next read? */

#define WAIT_FOR_CONN 1

/*
// -------------------------------------------------------------------------
// structs, typedefs and globals
// -------------------------------------------------------------------------
*/

static struct sockaddr_in sockin;
static int addr_size = sizeof(sockin);

typedef struct serv_s SERV;
typedef struct conn_s CONN;

struct conn_s {
    int fd;                     /* File descriptor for input and output. */
    FILE     * fp;              /* file pointer */
    char     * buf;             /* Buffer for network output. */
    int        dead;            /* Connection is defunct. */
    CONN     * next;
};

struct serv_s {
    int  s_socket;
    int  c_socket;
    char c_addr[20];
};

struct lookup_s {
    int    type;
    char   buf[BIGBUF];
    Bool   status;
};

jmp_buf  start_env;

int    req_error;

int    activity;

CONN * connections;
SERV   serv;

/*
// -------------------------------------------------------------------------
// prototypes
// -------------------------------------------------------------------------
*/

void flush_defunct(void);
void connection_discard(CONN * conn);
void uninit(void);
void catch_signal(int signal);
void init(int port, int * fd);
void flush_defunct(void);
int  my_io_event_wait(long sec);
CONN * connection_add(int fd);
void cwrite(CONN * conn, char * buf);

/*
// -------------------------------------------------------------------------
// functions
// -------------------------------------------------------------------------
*/

void write_err(char *fmt, ...) {
    va_list arg;

    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
}

/* ---------------------------------- */
/* what should I do with signals? */
void catch_signal(int signal) {
    switch (signal) {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            fprintf(stderr, "caught signal %d, cleaning up ...\n", signal);
            uninit();
            exit(0);
            break;
        case SIGHUP: {
            CONN * conn;

            fprintf(stderr, "caught SIGHUP, booting connections...\n");

            for (conn = connections; conn; conn = conn->next)
                conn->dead = 1;

            flush_defunct();

            longjmp(start_env, 0);

            break;
        }
    }
}

void init(int port, int * fd) {
    int one=1;

    /* ------------------------------------------------- */
    /* setup signal handling */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  catch_signal);
    signal(SIGQUIT, catch_signal);
    signal(SIGHUP,  catch_signal);
    signal(SIGTERM, catch_signal);

    /* ------------------------------------------------- */
    /* Create a socket. */
    if ((*fd = socket(AF_INET, SOCK_STREAM, 0)) == F_FAILURE) {
        fprintf(stderr, "init(): socket(): %s\n", strerror(errno));
        exit(errno);
    }

    /* Set SO_REUSEADDR option to avoid restart problems. */
    setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int));

    /* Bind the socket to port. */
    memset(&sockin, 0, sizeof(sockin));
    sockin.sin_family = AF_INET;
    sockin.sin_port = htons(port);

    if (bind(*fd, (struct sockaddr *) &sockin, sizeof(sockin)) == F_FAILURE) {
        fprintf(stderr, "init(): bind(): %s\n", strerror(errno));
        exit(errno);
    }

    /*
    // Start listening on port.
    // This should not return an error under any circumstances.
    //
    // (i.e. the failures of listen() will not occur because we are using
    //  it correctly)
    */
    listen(*fd, CONN_QUEUE);
    fprintf(stderr, "listening to port %d\n", port);
}

void flush_defunct(void) {
    CONN **connp, *conn;

    connp = &connections;
    while (*connp) {
        conn = *connp;
        if (conn->dead) {
            *connp = conn->next;
            connection_discard(conn);
        } else {  
            connp = &conn->next;
        }
    }
}

int handle_request (char * req, CONN * conn) {
    char    dnsbuf[DNS_MAXLEN+1],
            code,
          * end,
          * id;

    while (*req && *req == ' ')
        req++;
    if (strlen(req) == 0) {
        req_error = 1;
        return F_FAILURE;
    }

    /* trim the end */
    end = &req[strlen(req) - 1];
    while (end > req && (*end == ' ' || *end == '\n' || *end == '\r'))
        end--;
    *(end+1) = (char) NULL;

    /* get the id */
    id = req;
    while (*req && isdigit(*req))
        req++;
    if (*req != ':') {
        req_error = 2;
        return F_FAILURE;
    }

    *req = (char) NULL;
    req++;
    code = *req;
    if (!code) {
        req_error = 3;
        return F_FAILURE;
    }
    req++;
    if (*req != ':') {
        req_error = 4;
        return F_FAILURE;
    }
    *req = (char) NULL;
    req++;

    switch (code) {
        case 'N': case 'n':
            switch (lookup_name_by_ip(req, dnsbuf)) {
                case DNS_INVADDR:
                    cwrite(conn, id);
                    cwrite(conn, ":N:Invalid IP Address: ");
                    cwrite(conn, req);
                    cwrite(conn, "\n");
                    break;
                case DNS_NORESOLV:
                    cwrite(conn, id);
                    cwrite(conn, ":F:No name for IP Address ");
                    cwrite(conn, req);
                    cwrite(conn, "\n");
                    break;
                case DNS_OVERFLOW:
                    cwrite(conn, id);
                    cwrite(conn, ":F:DNS Response overflows DNS_MAXLEN!\n");
                    break;
                default:
                    cwrite(conn, id);
                    cwrite(conn, ":G:");
                    cwrite(conn, dnsbuf);
                    cwrite(conn, "\n");
                    break;
            }
            break;
        case 'R': case 'r':
            switch (lookup_ip_by_name(req, dnsbuf)) {
                case DNS_NORESOLV:
                    cwrite(conn, id);
                    cwrite(conn, ":F:No name for IP Address ");
                    cwrite(conn, req);
                    cwrite(conn, "\n");
                    break;
                case DNS_OVERFLOW:
                    cwrite(conn, id);
                    cwrite(conn, ":F:DNS Response overflows DNS_MAXLEN!\n");
                    break;
                default:
                    cwrite(conn, id);
                    cwrite(conn, ":G:");
                    cwrite(conn, dnsbuf);
                    cwrite(conn, "\n");
                    break;
            }
            break;
        default:
            cwrite(conn, id);
            cwrite(conn, ":F:Unknown Request Type: ");
            write(conn->fd, &code, 1);
            cwrite(conn, "\n");
    }

    return F_SUCCESS;
}

void handle_io(void) {
    char * p;
    CONN * conn;
    char   linebuf[BIGBUF+1];
    FILE * fp;

    /* read from the sockets now ... */
    for (conn = connections; conn; conn = conn->next) {
        fflush(stdout);
        fp = conn->fp;

        /* read a line */
        if (feof(fp)) {
            conn->dead = 1;
            continue;
        }

        if (!(p = fgets(linebuf, BIGBUF, fp))) {
            if (errno != EAGAIN) {
                fprintf(stderr, "fgets(): %s\n", strerror(errno));
                conn->dead = 1;
            }
            errno = 0;
            continue;
        }

        activity++;

        if (handle_request(p, conn) == F_FAILURE)
            fprintf(stderr, "Invalid Request Format, Error Number %d\n",
                             req_error);
    }
}

int main (void) {
    serv.s_socket = -1;

    /* initialize the server */
    init(NET_PORT, &serv.s_socket);

    setjmp(start_env);

    for (;;) {
        /* wait for new, flush defunct and blast em */
        my_io_event_wait(WAIT_FOR_CONN);
        flush_defunct();

        /* get input, if there is none, sleep a second and try again */
        /* we need the sleep, could load the machine without it */
        /* need to rewrite io_event_wait() to better use select instead */
        activity=0;
        handle_io();
        if (!activity)
            sleep(1);
    }

    uninit();

    return 0;
}

void connection_discard(CONN * conn) {

    if (conn == NULL)
        return;

    fclose(conn->fp);
    close(conn->fd);
    FREE(conn);
}

void cwrite(CONN * conn, char * buf) {
   if (write(conn->fd, buf, strlen(buf)) == F_FAILURE) {
       fprintf(stderr, "write(): %s\n", strerror(errno));
       conn->dead = 1;
   }
}

CONN * connection_add(int fd) {
    CONN * conn;
    int    tmp;

    /* add it to the list of connections */
    conn = MALLOC(CONN, 1);
    conn->fd = fd;
    conn->fp = fdopen(fd, "ab+");
    if (!conn->fp)
        fprintf(stderr, "fdopen(): %s\n", strerror(errno));
    conn->dead = 0; 
    conn->next = connections;
    connections = conn;

    /* set the socket as non-blocking */
    tmp = fcntl(fd, F_GETFL);
    tmp |= O_NONBLOCK;
    fcntl(fd, F_SETFL, tmp);
            
    return conn;
}

void uninit(void) {
    CONN **connp, *conn;

    connp = &connections;
    while (*connp) {
        conn = *connp;
        *connp = conn->next;
        connection_discard(conn);
    }
}

int my_io_event_wait(long sec) {
    struct timeval   tv,
                   * tvp;
    CONN           * conn;
    fd_set           read_fds,
                     write_fds;
    int              nfds,
                     count;

    if (sec == -1) {
        fprintf(stderr, "io_event_wait(): forever wait\n");
        exit(1);
    }

    tv.tv_sec = sec;
    tv.tv_usec = 0;
    tvp = &tv;

    /* Begin with blank file descriptor masks and an nfds of 0. */
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    nfds = 0;

    /* current connections... */
    for (conn = connections; conn; conn = conn->next) {
        if (!conn->dead)
            FD_SET(conn->fd, &write_fds);
	if (conn->fd >= nfds)
	    nfds = conn->fd + 1;
    }

    FD_SET(serv.s_socket, &read_fds);
    if (serv.s_socket >= nfds)
        nfds = serv.s_socket + 1;

    /* Call select(). */
    count = select(nfds, &read_fds, &write_fds, NULL, tvp);

    /* Lose horribly if select() fails on anything but an
       interrupted system call.  On EINTR, we'll return 0. */
    if (count == F_FAILURE && errno != EINTR) {
	fprintf(stderr, "io_event_wait(): select(): %s\n", strerror(errno));
        exit(1);
    }

    /* no I/O events occurred */
    if (count <= 0)
	return F_FAILURE;

    /* see if anything died on us .. */
    for (conn = connections; conn; conn = conn->next) {
        if (!FD_ISSET(conn->fd, &write_fds))
            conn->dead = 1;
    }

    /* new connections? */
    if (FD_ISSET(serv.s_socket, &read_fds)) {
        serv.c_socket = accept(serv.s_socket,
                                (struct sockaddr *) &sockin,
                                &addr_size);
        if (serv.c_socket >= 0) {
            strcpy(serv.c_addr, inet_ntoa(sockin.sin_addr));

            conn = connection_add(serv.c_socket);
            fprintf(stderr, "Connection from %s\n", serv.c_addr);

            serv.c_socket = -1;
            serv.c_addr[0] = (char) NULL;

        }
    }

    /* at least one I/O event occurred */
    return F_SUCCESS;
}

