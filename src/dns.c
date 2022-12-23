/*
// Full copyright information is available in the file ../doc/CREDITS
//
// RFC references: 1293, 903, 1035
*/

#include "defs.h"

#ifdef __UNIX__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <ctype.h>
#include "dns.h"

/* out must be a DNS_MAXLEN character buffer */
int lookup_name_by_ip(const char * ip, char * out)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int errcode;

    memset(&addr, 0, sizeof(addr));

    // For now, we only do IPv4.
    addr.ss_family = AF_INET;
    struct sockaddr_in *addr4 = (struct sockaddr_in*)&addr;
    addrlen = sizeof(*addr4);
    errcode = inet_pton(AF_INET, ip, &addr4->sin_addr);

    if (errcode == 0) {
        return DNS_INVADDR;
    }

    errcode = getnameinfo((struct sockaddr *)&addr, addrlen, out, DNS_MAXLEN, NULL, 0, 0);
    if (errcode == EAI_OVERFLOW) {
        return DNS_OVERFLOW;
    } else if (errcode != 0) {
        return DNS_NORESOLV;
    }

    return DNS_NOERROR;
}

/* out must be a INET6_ADDRSTRLEN character buffer */
int lookup_ip_by_name(const char * name, char * out)
{
    struct addrinfo hints, *result;
    int errcode;

    memset(&hints, 0, sizeof(hints));
    // Only do IPv4 for now.
    hints.ai_family = AF_INET;

    errcode = getaddrinfo(name, NULL, &hints, &result);
    if (errcode != 0) {
        return DNS_NORESOLV;
    }

    // While we only support IPv4 for now, this area is ready for
    // the future. Maybe.
    switch (result->ai_family) {
        case AF_INET: {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)result->ai_addr;
            inet_ntop(AF_INET, &(addr_in->sin_addr), out, INET6_ADDRSTRLEN);
            break;
        }
        case AF_INET6: {
            struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)result->ai_addr;
            inet_ntop(AF_INET6, &(addr_in6->sin6_addr), out, INET6_ADDRSTRLEN);
            break;
        }
        default:
            freeaddrinfo(result);
            return DNS_NORESOLV;
    }

    freeaddrinfo(result);
    return DNS_NOERROR;
}

