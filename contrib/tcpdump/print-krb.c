/*
 * Copyright (c) 1995, 1996, 1997
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
 *
 * Initial contribution from John Hawkinson (jhawk@mit.edu).
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-krb.c,v 1.23 2003-11-16 09:36:26 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

static packetbody_t c_print(packetbody_t, packetbody_t);
static packetbody_t krb4_print_hdr(packetbody_t);
static void krb4_print(packetbody_t);

#define AUTH_MSG_KDC_REQUEST			1<<1
#define AUTH_MSG_KDC_REPLY			2<<1
#define AUTH_MSG_APPL_REQUEST			3<<1
#define AUTH_MSG_APPL_REQUEST_MUTUAL		4<<1
#define AUTH_MSG_ERR_REPLY			5<<1
#define AUTH_MSG_PRIVATE			6<<1
#define AUTH_MSG_SAFE				7<<1
#define AUTH_MSG_APPL_ERR			8<<1
#define AUTH_MSG_DIE				63<<1

#define KERB_ERR_OK				0
#define KERB_ERR_NAME_EXP			1
#define KERB_ERR_SERVICE_EXP			2
#define KERB_ERR_AUTH_EXP			3
#define KERB_ERR_PKT_VER			4
#define KERB_ERR_NAME_MAST_KEY_VER		5
#define KERB_ERR_SERV_MAST_KEY_VER		6
#define KERB_ERR_BYTE_ORDER			7
#define KERB_ERR_PRINCIPAL_UNKNOWN		8
#define KERB_ERR_PRINCIPAL_NOT_UNIQUE		9
#define KERB_ERR_NULL_KEY			10

struct krb {
	u_int8_t pvno;		/* Protocol Version */
	u_int8_t type;		/* Type+B */
};

static char tstr[] = " [|kerberos]";

static struct tok type2str[] = {
	{ AUTH_MSG_KDC_REQUEST,		"KDC_REQUEST" },
	{ AUTH_MSG_KDC_REPLY,		"KDC_REPLY" },
	{ AUTH_MSG_APPL_REQUEST,	"APPL_REQUEST" },
	{ AUTH_MSG_APPL_REQUEST_MUTUAL,	"APPL_REQUEST_MUTUAL" },
	{ AUTH_MSG_ERR_REPLY,		"ERR_REPLY" },
	{ AUTH_MSG_PRIVATE,		"PRIVATE" },
	{ AUTH_MSG_SAFE,		"SAFE" },
	{ AUTH_MSG_APPL_ERR,		"APPL_ERR" },
	{ AUTH_MSG_DIE,			"DIE" },
	{ 0,				NULL }
};

static struct tok kerr2str[] = {
	{ KERB_ERR_OK,			"OK" },
	{ KERB_ERR_NAME_EXP,		"NAME_EXP" },
	{ KERB_ERR_SERVICE_EXP,		"SERVICE_EXP" },
	{ KERB_ERR_AUTH_EXP,		"AUTH_EXP" },
	{ KERB_ERR_PKT_VER,		"PKT_VER" },
	{ KERB_ERR_NAME_MAST_KEY_VER,	"NAME_MAST_KEY_VER" },
	{ KERB_ERR_SERV_MAST_KEY_VER,	"SERV_MAST_KEY_VER" },
	{ KERB_ERR_BYTE_ORDER,		"BYTE_ORDER" },
	{ KERB_ERR_PRINCIPAL_UNKNOWN,	"PRINCIPAL_UNKNOWN" },
	{ KERB_ERR_PRINCIPAL_NOT_UNIQUE,"PRINCIPAL_NOT_UNIQUE" },
	{ KERB_ERR_NULL_KEY,		"NULL_KEY"},
	{ 0,				NULL}
};

static packetbody_t
c_print(packetbody_t s, packetbody_t ep)
{
	register u_char c;
	register int flag;

	flag = 1;
	while (s < ep) {
		c = *s++;
		if (c == '\0') {
			flag = 0;
			break;
		}
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
	if (flag)
		return NULL;
	return (s);
}

static packetbody_t
krb4_print_hdr(packetbody_t cp)
{
	cp += 2;

#define PRINT		if ((cp = c_print(cp, snapend)) == NULL) goto trunc

	PRINT;
	putchar('.');
	PRINT;
	putchar('@');
	PRINT;
	return (cp);

trunc:
	fputs(tstr, stdout);
	return (NULL);

#undef PRINT
}

static void
krb4_print(packetbody_t cp)
{
	__capability const struct krb *kp;
	u_char type;
	u_short len;

#define PRINT		if ((cp = c_print(cp, snapend)) == NULL) goto trunc
/*  True if struct krb is little endian */
#define IS_LENDIAN(kp)	(((kp)->type & 0x01) != 0)
#define KTOHSP(kp, cp)	(IS_LENDIAN(kp) ? EXTRACT_LE_16BITS(cp) : EXTRACT_16BITS(cp))

	kp = (__capability const struct krb *)cp;

	if (!PACKET_HAS_ELEMENT(kp, type)) {
		fputs(tstr, stdout);
		return;
	}

	type = kp->type & (0xFF << 1);

	printf(" %s %s: ",
	    IS_LENDIAN(kp) ? "le" : "be", tok2str(type2str, NULL, type));

	switch (type) {

	case AUTH_MSG_KDC_REQUEST:
		if ((cp = krb4_print_hdr(cp)) == NULL)
			return;
		cp += 4;	/* ctime */
		PACKET_HAS_ONE_OR_TRUNC(cp);
		printf(" %dmin ", *cp++ * 5);
		PRINT;
		putchar('.');
		PRINT;
		break;

	case AUTH_MSG_APPL_REQUEST:
		cp += 2;
		PACKET_HAS_ONE_OR_TRUNC(cp);
		printf("v%d ", *cp++);
		PRINT;
		PACKET_HAS_ONE_OR_TRUNC(cp);
		printf(" (%d)", *cp++);
		PACKET_HAS_ONE_OR_TRUNC(cp);
		printf(" (%d)", *cp);
		break;

	case AUTH_MSG_KDC_REPLY:
		if ((cp = krb4_print_hdr(cp)) == NULL)
			return;
		cp += 10;	/* timestamp + n + exp + kvno */
		PACKET_HAS_SPACE_OR_TRUNC(cp, sizeof(short));
		len = KTOHSP(kp, cp);
		printf(" (%d)", len);
		break;

	case AUTH_MSG_ERR_REPLY:
		if ((cp = krb4_print_hdr(cp)) == NULL)
			return;
		cp += 4; 	  /* timestamp */
		PACKET_HAS_SPACE_OR_TRUNC(cp, sizeof(short));
		printf(" %s ", tok2str(kerr2str, NULL, KTOHSP(kp, cp)));
		cp += 4;
		PRINT;
		break;

	default:
		fputs("(unknown)", stdout);
		break;
	}

	return;
trunc:
	fputs(tstr, stdout);
}

void
krb_print(packetbody_t dat)
{
	__capability const struct krb *kp;

	kp = (__capability const struct krb *)dat;

	if (!PACKET_HAS_ELEMENT(kp, pvno)) {
		fputs(tstr, stdout);
		return;
	}

	switch (kp->pvno) {

	case 1:
	case 2:
	case 3:
		printf(" v%d", kp->pvno);
		break;

	case 4:
		printf(" v%d", kp->pvno);
		krb4_print((packetbody_t)kp);
		break;

	case 106:
	case 107:
		fputs(" v5", stdout);
		/* Decode ASN.1 here "someday" */
		break;
	}
	return;
}
