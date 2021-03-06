/*
 * Copyright (c) 2000 Ben Smithurst <ben@scientia.demon.co.uk>
 * All rights reserved.
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
    "@(#) $Header: /tcpdump/master/tcpdump/print-timed.c,v 1.9 2003-11-16 09:36:40 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>

#include "timed.h"
#include "interface.h"
#include "extract.h"

static const char *tsptype[TSPTYPENUMBER] =
  { "ANY", "ADJTIME", "ACK", "MASTERREQ", "MASTERACK", "SETTIME", "MASTERUP",
  "SLAVEUP", "ELECTION", "ACCEPT", "REFUSE", "CONFLICT", "RESOLVE", "QUIT",
  "DATE", "DATEREQ", "DATEACK", "TRACEON", "TRACEOFF", "MSITE", "MSITEREQ",
  "TEST", "SETDATE", "SETDATEREQ", "LOOP" };

void
timed_print(packetbody_t bp)
{
	__capability const struct tsp *tsp = (__capability const struct tsp *)bp;
	long sec, usec;
	packetbody_t end;

	if (!PACKET_HAS_ELEMENT(tsp, tsp_type)) {
		fputs("[|timed]", stdout);
		return;
	}
	if (tsp->tsp_type < TSPTYPENUMBER)
		printf("TSP_%s", tsptype[tsp->tsp_type]);
	else
		printf("(tsp_type %#x)", tsp->tsp_type);

	if (!PACKET_HAS_ELEMENT(tsp, tsp_vers)) {
		fputs(" [|timed]", stdout);
		return;
	}
	printf(" vers %d", tsp->tsp_vers);

	if (!PACKET_HAS_ELEMENT(tsp, tsp_seq)) {
		fputs(" [|timed]", stdout);
		return;
	}
	printf(" seq %d", tsp->tsp_seq);

	if (tsp->tsp_type == TSP_LOOP) {
		if (!PACKET_HAS_ELEMENT(tsp, tsp_hopcnt)) {
			fputs(" [|timed]", stdout);
			return;
		}
		printf(" hopcnt %d", tsp->tsp_hopcnt);
	} else if (tsp->tsp_type == TSP_SETTIME ||
	  tsp->tsp_type == TSP_ADJTIME ||
	  tsp->tsp_type == TSP_SETDATE ||
	  tsp->tsp_type == TSP_SETDATEREQ) {
		if (!PACKET_HAS_ELEMENT(tsp, tsp_time)) {
			fputs(" [|timed]", stdout);
			return;
		}
		sec = EXTRACT_32BITS(&tsp->tsp_time.tv_sec);
		usec = EXTRACT_32BITS(&tsp->tsp_time.tv_usec);
		if (usec < 0)
			/* corrupt, skip the rest of the packet */
			return;
		fputs(" time ", stdout);
		if (sec < 0 && usec != 0) {
			sec++;
			if (sec == 0)
				fputc('-', stdout);
			usec = 1000000 - usec;
		}
		printf("%ld.%06ld", sec, usec);
	}

	end = p_memchr(tsp->tsp_name, '\0', PACKET_REMAINING(tsp->tsp_name));
	if (end == NULL)
		fputs(" [|timed]", stdout);
	else {
		fputs(" name ", stdout);
		fn_print(tsp->tsp_name, end);
	}
}
