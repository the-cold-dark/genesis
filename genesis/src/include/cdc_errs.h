/*
// Full copyright information is available in the file ../doc/CREDITS
//
// because Win32 defines errors just slightly different, and the same errors
// are WSA errors if used in regard to sockets, in win32
*/

#ifdef __Win32__

#include <errno.h>

/* In VC, linked to winsock.h, is a header that defines FLOAT, this keeps it from
   compaining and prevents it from interfering with genesis */
#ifdef __MSVC__
#undef FLOAT
#endif

#include <winsock.h>

/* Undefine VC's silly definition */
#ifdef __MSVC__
#undef FLOAT
#endif

#define     ERR_INTR         WSAEINTR
#define     ERR_NOMEM        ENOMEM
#define     ERR_RANGE        ERANGE
#define     ERR_INPROGRESS   WSAEINPROGRESS
#define     ERR_NETUNREACH   WSAENETUNREACH
#define     ERR_TIMEDOUT     WSAETIMEDOUT
#define     ERR_CONNREFUSED  WSAECONNREFUSED
#define     ERR_AGAIN        WSAEWOULDBLOCK

#define GETERR()      GetLastError()
#define SETERR(_err_) SetLastError(_err_)

#else

#include <errno.h>

#define     ERR_INTR         EINTR
#define     ERR_NOMEM        ENOMEM
#define     ERR_RANGE        ERANGE
#define     ERR_INPROGRESS   EINPROGRESS
#define     ERR_NETUNREACH   ENETUNREACH
#define     ERR_TIMEDOUT     ETIMEDOUT
#define     ERR_CONNREFUSED  ECONNREFUSED
#define     ERR_AGAIN        EAGAIN

#define GETERR()      errno
#define SETERR(_err_) errno = _err_

#endif
