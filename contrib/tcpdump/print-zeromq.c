/*
 * This file implements decoding of ZeroMQ network protocol(s).
 *
 *
 * Copyright (c) 2013 The TCPDUMP project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>

#include "interface.h"
#include "extract.h"

/* Maximum number of ZMTP/1.0 frame body bytes (without the flags) to dump in
 * hex and ASCII under a single "-v" flag.
 */
#define VBYTES 128

/*
 * Below is an excerpt from the "13/ZMTP" specification:
 *
 * A ZMTP message consists of 1 or more frames.
 *
 * A ZMTP frame consists of a length, followed by a flags field and a frame
 * body of (length - 1) octets. Note: the length includes the flags field, so
 * an empty frame has a length of 1.
 *
 * For frames with a length of 1 to 254 octets, the length SHOULD BE encoded
 * as a single octet. The minimum valid length of a frame is 1 octet, thus a
 * length of 0 is invalid and such frames SHOULD be discarded silently.
 *
 * For frames with lengths of 255 and greater, the length SHALL BE encoded as
 * a single octet with the value 255, followed by the length encoded as a
 * 64-bit unsigned integer in network byte order. For frames with lengths of
 * 1 to 254 octets this encoding MAY be also used.
 *
 * The flags field consists of a single octet containing various control
 * flags. Bit 0 is the least significant bit.
 *
 * - Bit 0 (MORE): More frames to follow. A value of 0 indicates that there
 *   are no more frames to follow. A value of 1 indicates that more frames
 *   will follow. On messages consisting of a single frame the MORE flag MUST
 *   be 0.
 *
 * - Bits 1-7: Reserved. Bits 1-7 are reserved for future use and SHOULD be
 *   zero.
 */

static packetbody_t
zmtp1_print_frame(packetbody_t cp, packetbody_t ep) {
	u_int64_t body_len_declared, body_len_captured, header_len;
	u_int8_t flags;

	printf("\n\t");
	PACKET_HAS_SPACE_OR_TRUNC(cp, 1); /* length/0xFF */

	if (cp[0] != 0xFF) {
		header_len = 1; /* length */
		body_len_declared = cp[0];
		if (body_len_declared == 0)
			return cp + header_len; /* skip to next frame */
		printf(" frame flags+body  (8-bit) length %"PRIu8"", cp[0]);
		PACKET_HAS_SPACE_OR_TRUNC(cp, header_len + 1); /* length, flags */
		flags = cp[1];
	} else {
		header_len = 1 + 8; /* 0xFF, length */
		printf(" frame flags+body (64-bit) length");
		PACKET_HAS_SPACE_OR_TRUNC(cp, header_len); /* 0xFF, length */
		body_len_declared = EXTRACT_64BITS(cp + 1);
		if (body_len_declared == 0)
			return cp + header_len; /* skip to next frame */
		printf(" %"PRIu64"", body_len_declared);
		PACKET_HAS_SPACE_OR_TRUNC(cp, header_len + 1); /* 0xFF, length, flags */
		flags = cp[9];
	}

	body_len_captured = ep - cp - header_len;
	if (body_len_declared > body_len_captured)
		printf(" (%"PRIu64" captured)", body_len_captured);
	printf(", flags 0x%02"PRIx8"", flags);

	if (vflag) {
		u_int64_t body_len_printed = MIN(body_len_captured, body_len_declared);

		printf(" (%s|%s|%s|%s|%s|%s|%s|%s)",
			flags & 0x80 ? "MBZ" : "-",
			flags & 0x40 ? "MBZ" : "-",
			flags & 0x20 ? "MBZ" : "-",
			flags & 0x10 ? "MBZ" : "-",
			flags & 0x08 ? "MBZ" : "-",
			flags & 0x04 ? "MBZ" : "-",
			flags & 0x02 ? "MBZ" : "-",
			flags & 0x01 ? "MORE" : "-");

		if (vflag == 1)
			body_len_printed = MIN(VBYTES + 1, body_len_printed);
		if (body_len_printed > 1) {
			printf(", first %"PRIu64" byte(s) of body:", body_len_printed - 1);
			hex_and_ascii_print("\n\t ", cp + header_len + 1, body_len_printed - 1);
			printf("\n");
		}
	}

	PACKET_HAS_SPACE_OR_TRUNC(cp, header_len + body_len_declared); /* Next frame within the buffer ? */
	return cp + header_len + body_len_declared;

trunc:
	printf(" [|zmtp1]");
	return ep;
}

void
zmtp1_print(packetbody_t cp, u_int len) {
	packetbody_t ep = PACKET_SECTION_END(cp, len);

	printf(": ZMTP/1.0");
	while (cp < ep)
		cp = zmtp1_print_frame(cp, ep);
}

