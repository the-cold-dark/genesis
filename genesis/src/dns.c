/*
// Full copyright information is available in the file ../doc/CREDITS
//
// RFC references: 1293, 903, 1035
*/

#define _BSD 44 /* For RS6000s. */

#define _dns_c_

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
int lookup_name_by_ip(char * ip, char * out)
{
   unsigned addr;
   register struct hostent * hp;

   addr = inet_addr(ip);
   if (addr == INVALID_INADDR)
       return DNS_INVADDR;

   if (!(hp = gethostbyaddr((char *) &addr, 4, AF_INET)))
       return DNS_NORESOLV;

   /* we have a problem houston */
   strncpy(out, hp->h_name, DNS_MAXLEN);
   if (strlen(hp->h_name) > DNS_MAXLEN) {
       write_err("Hostname longer than DNS_MAXLEN?!?: '%s'\n", hp->h_name);
       out[DNS_MAXLEN] = (char) NULL;
       return DNS_OVERFLOW;
   }
   return DNS_NOERROR;
}

/* out must be a DNS_MAXLEN character buffer */
int lookup_ip_by_name(char * name, char * out)
{
   register struct hostent *hp;
   char * p;

   if (!(hp = gethostbyname(name)))
       return DNS_NORESOLV;

   p = inet_ntoa(*(struct in_addr *)hp->h_addr);
   strncpy(out, p, DNS_MAXLEN);
   if (strlen(p) > DNS_MAXLEN) {
       write_err("Hostname longer than DNS_MAXLEN?!?: '%s'\n", hp->h_name);
       out[DNS_MAXLEN] = (char) NULL;
       return DNS_OVERFLOW;
   }
   return DNS_NOERROR;
}

