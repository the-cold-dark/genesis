/*
// Full copyright information is available in the file ../doc/CREDITS
//
// RFC references: 1293, 903, 1035
*/

#ifndef _dns_h_
#define _dns_h_

#ifdef __UNIX__
#define INVALID_INADDR F_FAILURE
#else
#define INVALID_INADDR INADDR_NONE
#endif

#define DNS_NOERROR		0
#define DNS_INVADDR		1
#define DNS_NORESOLV		2
#define DNS_OVERFLOW		3

/* RFC 1035 defines the maximum length of a name as 255 octets */
#define DNS_MAXLEN		255

#ifndef _dns_c_
extern int dns_error;
#else
int dns_error;
#endif

int lookup_name_by_ip(char * ip, char * out);
int lookup_ip_by_name(char * name, char * out);

#endif

