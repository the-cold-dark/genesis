
#define _SHS_include_

#include "defs.h"

#include <ctype.h>
#include "execute.h"
#include "util.h"
#include "crypt.h"

static uChar ascii64[] =        /* 0 ... 63 => ascii - 64 */
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/*
// crypt() is not POSIX--this is here for backwards compatability,
// nasty word that.
*/
#ifdef USE_OS_CRYPT
extern char * crypt(const char *, const char *);
#endif

/*
// Encrypt a string.  The salt can be NULL--force SHS encryption,
// match_crypted() will handle older DES passwords
*/
cStr * strcrypt(cStr * key, cStr * salt) {
    char   pwd_buf[SHS_OUTPUT_SIZE];  /* output buffer for the password */
    uChar * pp, * sp, rsalt[9];         /* 8 chars of salt, one NULL */
    Int    x,
           pl,
           sl;

    if (!salt) {
        random_salt:

        for (x=0; x < 8; x++)
            rsalt[x] = ascii64[random_number(64)];
        rsalt[8] = (uChar) NULL;
        sp = rsalt;
        sl = 8;
    } else {
        sp = (uChar *) string_chars(salt);
        if (sp[0] == '$' && sp[1] == '2' && sp[2] == '$') {
            sp += 3;
            for (x=0; x < 8 && sp[x] != '$'; x++)
                rsalt[x] = sp[x];
            rsalt[x] = (uChar) NULL;
            sp = rsalt;
            sl = strlen((char *) sp);
            if (!sl)
                goto random_salt;
        } else {
            sl = string_length(salt);
        }
    }

    pp = (uChar *) string_chars(key);
    pl = string_length(key);

    shs_crypt(pp, pl, sp, sl, pwd_buf);

    return string_from_chars(pwd_buf, strlen(pwd_buf));
}   

/*
// match the encrypted string, use SHS (default for crypt()) if
// 'encrypted' begins with "$2$" (FreeBSD standard); otherwise
// pass back to the OS crypt()
*/
Int match_crypted(cStr * encrypted, cStr * possible) {
    uChar * ep, * pp, *sp, salt[9];
    char   p_buf[SHS_OUTPUT_SIZE];
    Int    sl,
           el,
           pl,
           x;

    ep = (uChar *) string_chars(encrypted);
    el = string_length(encrypted);
    pp = (uChar *) string_chars(possible);
    pl = string_length(possible);

    if (el < 3) {
        cthrow(type_id, "Invalid password format");
        return -1;
    }

    if (ep[0] == '$' && ep[1] == '2' && ep[2] == '$') {
        sp = ep + 3;
        for (x=0; x < 8 && sp[x] != '$'; x++)
            salt[x] = sp[x];
        salt[x] = (uChar) NULL;
        sp = salt;
        sl = strlen((char *) sp);

        shs_crypt((uChar *) pp, pl, sp, sl, p_buf);

        return (!strcmp((char *) ep, p_buf));
    } else {
#ifdef USE_OS_CRYPT
#ifdef sys_freebsd
        sp = ep;
#else
        /* assume ancient DES format, with the first two chars as salt */
        salt[0] = ep[0];
        salt[1] = ep[1];
        salt[2] = (uChar) NULL;
        sp = salt;
#endif
        return
            (!strcmp((char *) ep, (char *) crypt((char *) pp, (char *) sp)));
#else
        cthrow(type_id, "Driver was not compiled with OS crypt() support.");
        return -1;
#endif
    }
}

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: crypt.c,v 1.33 2001/01/07 00:45:05 braddr Exp $
 *
 ***
 *** This license somewhat applies to shs_crypt() as its based off PHK's
 *** original MD5 crypt for FreeBSD -- Brandon Gillespie
 ***
 *
 */

void to64(uChar *s, uInt v, Int n) {
        while (--n >= 0) {
                *s++ = ascii64[v&0x3f];
                v >>= 6;
        }
}

char * shs_crypt(const unsigned char * pw,
                 const Int pl,
                 const unsigned char * sp,
                 const Int sl,
                 char * passwd)
{
    uChar *p;
    uChar    final[SHS_DIGEST_SIZE];
    Int i,j;
    SHS_CTX    ctx,ctx1;
    uInt l;

    shsInit(&ctx);

    /* The password first, since that is what is most unknown */
    shsUpdate(&ctx,pw,pl);

    /* Then our magic string */
    shsUpdate(&ctx,(uChar *)"$2$",3);

    /* Then the raw salt */
    shsUpdate(&ctx,sp,sl);

    /* Then just as many characters of the shs(pw,salt,pw) */
    shsInit(&ctx1);
    shsUpdate(&ctx1,pw,pl);
    shsUpdate(&ctx1,sp,sl);
    shsUpdate(&ctx1,pw,pl);
    shsFinal(&ctx1,final);
    for(i = pl; i > 0; i -= SHS_DIGEST_SIZE)
        shsUpdate(&ctx,final,i>SHS_DIGEST_SIZE ? SHS_DIGEST_SIZE : i);

    /* Don't leave anything around in vm they could use. */
    memset(final,0,sizeof final);

    /* Then something really weird... */
    for (j=0,i = pl; i ; i >>= 1)
        if(i&1)
            shsUpdate(&ctx, final+j, 1);
        else
            shsUpdate(&ctx, pw+j, 1);

    /* Now make the output string */
    strcpy(passwd, "$2$");
    strncat(passwd, (char *)sp, (Int)sl);
    strcat(passwd, "$");

    shsFinal(&ctx,final);

    /*
     * and now, just to make sure things don't run too fast
     * On a 60 Mhz Pentium this takes 34 msec, so you would
     * need 30 seconds to build a 1000 entry dictionary...
     */
    for(i=0;i<1000;i++) {
        shsInit(&ctx1);
        if(i & 1)
            shsUpdate(&ctx1,pw,pl);
        else
            shsUpdate(&ctx1,final,SHS_DIGEST_SIZE);

        if(i % 3)
            shsUpdate(&ctx1,sp,sl);

        if(i % 7)
            shsUpdate(&ctx1,pw,pl);

        if(i & 1)
            shsUpdate(&ctx1,final,SHS_DIGEST_SIZE);
        else
            shsUpdate(&ctx1,pw,pl);
        shsFinal(&ctx1,final);
    }

    p = (uChar *) passwd + strlen(passwd);

    l = (final[ 0]<<16) | (final[ 6]<<8) | final[12]; to64(p,l,4); p += 4;
    l = (final[ 1]<<16) | (final[ 7]<<8) | final[13]; to64(p,l,4); p += 4;
    l = (final[ 2]<<16) | (final[ 8]<<8) | final[14]; to64(p,l,4); p += 4;
    l = (final[ 3]<<16) | (final[ 9]<<8) | final[15]; to64(p,l,4); p += 4;
    l = (final[ 4]<<16) | (final[10]<<8) | final[16]; to64(p,l,4); p += 4;
    l = (final[ 5]<<16) | (final[11]<<8) | final[17]; to64(p,l,4); p += 4;
    l =                   (final[18]<<8) | final[19]; to64(p,l,3); p += 3;

    *p = '\0';

    /* Don't leave anything around in vm they could use. */
    memset(final,0,sizeof final);

    return passwd;
}

#undef _crypt_c_
