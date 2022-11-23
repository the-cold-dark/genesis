/*
// Full copyright information is available in the file ../doc/CREDITS
//
// RFC references: 1293, 903, 1035
*/

#ifndef _dns_h_
#define _dns_h_

#define DNS_NOERROR                0
#define DNS_INVADDR                1
#define DNS_NORESOLV                2
#define DNS_OVERFLOW                3

/* RFC 1035 defines the maximum length of a name as 255 octets */
#define DNS_MAXLEN                255

int lookup_name_by_ip(const char * ip, char * out);
int lookup_ip_by_name(const char * name, char * out);

#endif

