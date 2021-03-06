/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/util.c,v 1.109 2007-01-29 09:59:42 hannes Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"

int32_t thiszone;		/* seconds offset from gmt to local time */

static char * ts_format(register int, register int);

static inline void
__fn_putchar(u_char c)
{
	if (!isascii(c)) {
		c = toascii(c);
		putchar('M');
		putchar('-');
	}
	if (!isprint(c)) {
		c ^= 0x40;	/* DEL to ?, others to alpha */
		putchar('^');
	}
	putchar(c);
}

/*
 * Print out a null-terminated filename (or other ascii string).
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 */
int
fn_print(packetbody_t s, packetbody_t ep)
{
	register int ret;
	register u_char c;

	ret = 1;			/* assume truncated */
	while (ep == NULL || s < ep) {
		c = *s++;
		if (c == '\0') {
			ret = 0;
			break;
		}
		__fn_putchar(c);
	}
	return(ret);
}

/*
 * Print out a counted filename (or other ascii string).
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 */
int
fn_printn(packetbody_t s, register u_int n, packetbody_t ep)
{
	register u_char c;

	while (n > 0 && (ep == NULL || s < ep)) {
		n--;
		c = *s++;
		__fn_putchar(c);
		putchar(c);
	}
	return (n == 0) ? 0 : 1;
}

/*
 * Print out a null-padded filename (or other ascii string).
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 */
int
fn_printzp(packetbody_t s, u_int n, packetbody_t ep)
{
	register int ret;
	register u_char c;

	ret = 1;			/* assume truncated */
	while (n > 0 && (ep == NULL || s < ep)) {
		n--;
		c = *s++;
		if (c == '\0') {
			ret = 0;
			break;
		}
		__fn_putchar(c);
	}
	return (n == 0) ? 0 : ret;
}

/* Print null-terminated non-packet data */
int
fn_print_str(const u_char *s)
{
	register int ret;
	register u_char c;

	ret = 1;			/* assume truncated */
	while (1) {
		c = *s++;
		if (c == '\0') {
			ret = 0;
			break;
		}
		__fn_putchar(c);
	}
	return(ret);
}

/*
 * Format the timestamp
 */
static char *
ts_format(register int sec, register int usec)
{
        static char buf[sizeof("00:00:00.000000")];
        (void)snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%06u",
               sec / 3600, (sec % 3600) / 60, sec % 60, usec);

        return buf;
}

/*
 * Print the timestamp
 */
void
ts_print(register const struct timeval *tvp)
{
	register int s;
	struct tm *tm;
	time_t Time;
	static unsigned b_sec;
	static unsigned b_usec;
	int d_usec;
	int d_sec;

	switch (tflag) {

	case 0: /* Default */
		s = (tvp->tv_sec + thiszone) % 86400;
                (void)printf("%s ", ts_format(s, tvp->tv_usec));
		break;

	case 1: /* No time stamp */
		break;

	case 2: /* Unix timeval style */
		(void)printf("%u.%06u ",
			     (unsigned)tvp->tv_sec,
			     (unsigned)tvp->tv_usec);
		break;

	case 3: /* Microseconds since previous packet */
        case 5: /* Microseconds since first packet */
		if (b_sec == 0) {
                        /* init timestamp for first packet */
                        b_usec = tvp->tv_usec;
                        b_sec = tvp->tv_sec;                        
                }

                d_usec = tvp->tv_usec - b_usec;
                d_sec = tvp->tv_sec - b_sec;
                
                while (d_usec < 0) {
                    d_usec += 1000000;
                    d_sec--;
                }

                (void)printf("%s ", ts_format(d_sec, d_usec));

                if (tflag == 3) { /* set timestamp for last packet */
                    b_sec = tvp->tv_sec;
                    b_usec = tvp->tv_usec;
                }
		break;

	case 4: /* Default + Date*/
		s = (tvp->tv_sec + thiszone) % 86400;
		Time = (tvp->tv_sec + thiszone) - s;
		tm = gmtime (&Time);
		if (!tm)
			printf("Date fail  ");
		else
			printf("%04d-%02d-%02d %s ",
                               tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                               ts_format(s, tvp->tv_usec));
		break;
	}
}

/*
 * Print a relative number of seconds (e.g. hold time, prune timer)
 * in the form 5m1s.  This does no truncation, so 32230861 seconds
 * is represented as 1y1w1d1h1m1s.
 */
void
relts_print(int secs)
{
	static const char *lengths[] = {"y", "w", "d", "h", "m", "s"};
	static const int seconds[] = {31536000, 604800, 86400, 3600, 60, 1};
	const char **l = lengths;
	const int *s = seconds;

	if (secs == 0) {
		(void)printf("0s");
		return;
	}
	if (secs < 0) {
		(void)printf("-");
		secs = -secs;
	}
	while (secs > 0) {
		if (secs >= *s) {
			(void)printf("%d%s", secs / *s, *l);
			secs -= (secs / *s) * *s;
		}
		s++;
		l++;
	}
}

/*
 *  this is a generic routine for printing unknown data;
 *  we pass on the linefeed plus indentation string to
 *  get a proper output - returns 0 on error
 */

int
print_unknown_data(packetbody_t cp, const char *ident, int len)
{
	if (len < 0) {
		printf("%sDissector error: print_unknown_data called with negative length",
		    ident);
		return(0);
	}
	if (!PACKET_VALID(cp)) {
		printf("%sDissector error: print_unknown_data called with pointer past end of packet",
		    ident);
		return(0);
	}
	if (PACKET_REMAINING(cp) < (size_t)len)
		len = PACKET_REMAINING(cp);
        hex_print(ident,cp,len);
	return(1); /* everything is ok */
}

/*
 * Convert a token value to a string; use "fmt" if not found.
 */
const char *
tok2strbuf(register const struct tok *lp, register const char *fmt,
	   register int v, char *buf, size_t bufsize)
{
	if (lp != NULL) {
		while (lp->s != NULL) {
			if (lp->v == v)
				return (lp->s);
			++lp;
		}
	}
	if (fmt == NULL)
		fmt = "#%d";

	(void)snprintf(buf, bufsize, fmt, v);
	return (const char *)buf;
}

/*
 * Convert a token value to a string; use "fmt" if not found.
 */
const char *
tok2str(register const struct tok *lp, register const char *fmt,
	register int v)
{
	static char buf[4][128];
	static int idx = 0;
	char *ret;

	ret = buf[idx];
	idx = (idx+1) & 3;
	return tok2strbuf(lp, fmt, v, ret, sizeof(buf[0]));
}

/*
 * Convert a bit token value to a string; use "fmt" if not found.
 * this is useful for parsing bitfields, the output strings are seperated
 * if the s field is positive.
 */
static char *
bittok2str_internal(register const struct tok *lp, register const char *fmt,
	   register int v, register int sep)
{
        static char buf[256]; /* our stringbuffer */
        int buflen=0;
        register int rotbit; /* this is the bit we rotate through all bitpositions */
        register int tokval;

	while (lp != NULL && lp->s != NULL) {
            tokval=lp->v;   /* load our first value */
            rotbit=1;
            while (rotbit != 0) {
                /*
                 * lets AND the rotating bit with our token value
                 * and see if we have got a match
                 */
		if (tokval == (v&rotbit)) {
                    /* ok we have found something */
                    buflen+=snprintf(buf+buflen, sizeof(buf)-buflen, "%s%s",
                                     lp->s, sep ? ", " : "");
                    break;
                }
                rotbit=rotbit<<1; /* no match - lets shift and try again */
            }
            lp++;
	}

        /* user didn't want string seperation - no need to cut off trailing seperators */
        if (!sep) {
            return (buf);
        }

        if (buflen != 0) { /* did we find anything */
            /* yep, set the the trailing zero 2 bytes before to eliminate the last comma & whitespace */
            buf[buflen-2] = '\0';
            return (buf);
        }
        else {
            /* bummer - lets print the "unknown" message as advised in the fmt string if we got one */
            if (fmt == NULL)
		fmt = "#%d";
            (void)snprintf(buf, sizeof(buf), fmt, v);
            return (buf);
        }
}

/*
 * Convert a bit token value to a string; use "fmt" if not found.
 * this is useful for parsing bitfields, the output strings are not seperated.
 */
char *
bittok2str_nosep(register const struct tok *lp, register const char *fmt,
	   register int v)
{
    return (bittok2str_internal(lp, fmt, v, 0));
}

/*
 * Convert a bit token value to a string; use "fmt" if not found.
 * this is useful for parsing bitfields, the output strings are comma seperated.
 */
char *
bittok2str(register const struct tok *lp, register const char *fmt,
	   register int v)
{
    return (bittok2str_internal(lp, fmt, v, 1));
}

/*
 * Convert a value to a string using an array; the macro
 * tok2strary() in <interface.h> is the public interface to
 * this function and ensures that the second argument is
 * correct for bounds-checking.
 */
const char *
tok2strary_internal(register const char **lp, int n, register const char *fmt,
	register int v)
{
	static char buf[128];

	if (v >= 0 && v < n && lp[v] != NULL)
		return lp[v];
	if (fmt == NULL)
		fmt = "#%d";
	(void)snprintf(buf, sizeof(buf), fmt, v);
	return (buf);
}

/*
 * Convert a 32-bit netmask to prefixlen if possible
 * the function returns the prefix-len; if plen == -1
 * then conversion was not possible;
 */

int
mask2plen(u_int32_t mask)
{
	u_int32_t bitmasks[33] = {
		0x00000000,
		0x80000000, 0xc0000000, 0xe0000000, 0xf0000000,
		0xf8000000, 0xfc000000, 0xfe000000, 0xff000000,
		0xff800000, 0xffc00000, 0xffe00000, 0xfff00000,
		0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000,
		0xffff8000, 0xffffc000, 0xffffe000, 0xfffff000,
		0xfffff800, 0xfffffc00, 0xfffffe00, 0xffffff00,
		0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0,
		0xfffffff8, 0xfffffffc, 0xfffffffe, 0xffffffff
	};
	int prefix_len = 32;

	/* let's see if we can transform the mask into a prefixlen */
	while (prefix_len >= 0) {
		if (bitmasks[prefix_len] == mask)
			break;
		prefix_len--;
	}
	return (prefix_len);
}

#ifdef INET6
int
mask62plen(packetbody_t mask)
{
	u_char bitmasks[9] = {
		0x00,
		0x80, 0xc0, 0xe0, 0xf0,
		0xf8, 0xfc, 0xfe, 0xff
	};
	int byte;
	int cidr_len = 0;

	for (byte = 0; byte < 16; byte++) {
		u_int bits;

		for (bits = 0; bits < (sizeof (bitmasks) / sizeof (bitmasks[0])); bits++) {
			if (mask[byte] == bitmasks[bits]) {
				cidr_len += bits;
				break;
			}
		}

		if (mask[byte] != 0xff)
			break;
	}
	return (cidr_len);
}
#endif /* INET6 */

void
safeputs(packetbody_t s, int maxlen)
{
	int idx = 0;

	while (*s && idx < maxlen) {
		safeputchar(*s);
                idx++;
		s++;
	}
}

void
safeputchar(int c)
{
	unsigned char ch;

	ch = (unsigned char)(c & 0xff);
	if (ch < 0x80 && isprint(ch))
		printf("%c", ch);
	else
		printf("\\0x%02x", ch);
}


const char *
inet_ntop_cap(int af, __capability const void * restrict src,
    char * restrict dst, socklen_t size)
{
	union {
		struct in_addr	in;
		struct in6_addr	in6;
	} addr;

	switch (af) {
	case AF_INET:
		p_memcpy_from_packet(&addr.in, src, sizeof(addr.in));
		break;
	case AF_INET6:
		p_memcpy_from_packet(&addr.in, src, sizeof(addr.in));
		break;
	default:
		return NULL;
	}
	return (inet_ntop(af, &addr, dst, size));
}
    
#ifdef HAS_CHERI_CAPABILITIES
char *
p_strdup(packetbody_t data) {
        char *str;
        size_t len;

        len = strnlen_c(data, PACKET_REMAINING(data)) + 1;
        if ((str = malloc(len)) != NULL) {
                strncpy_c_fromcap(str, data, len - 1);
		str[len] = '\0';
	}
        return (str);
}

char *
p_strndup(packetbody_t data, size_t n) {
	char *str;
	size_t len;

	len = strnlen_c(data, n) + 1;
	if ((str = malloc(len)) != NULL) {
		strncpy_c_fromcap(str, data, len - 1);
		str[len] = '\0';
	}
	return (str);
}
#endif
