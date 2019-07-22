/*-
 * Copyright (c) 2019 by Steve Jacobson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "sock.h"

// Fixme:  May not be portable.  Works for FreeBSD.
#ifndef htonll
#include <sys/endian.h>
#define htonll(x)		htobe64(x)
#endif

/*
 * These are the ficticious "Option X" and "Option Y" from Appendix A of
 * RFC 8200.  They have been assigned the following made-up values.
 */
#define HOPOPT_OPT_X		0x2a
#define HOPOPT_OPT_Y		0x2b

#define HOPOPT_OPT_X_HDR_LEN	12
#define HOPOPT_OPT_X_HDR_ALIGN	8

#define HOPOPT_OPT_Y_HDR_LEN	7
#define HOPOPT_OPT_Y_HDR_ALIGN	4

/*
 * "Option V" and "Option W" are imitations of "Option Y" and "Option X"
 * above.  They are basically the same format, but are placed in the
 * option header in the reverse order.  They have been assigned the
 * following made-up values.
 */
#define HOPOPT_OPT_V		0x28
#define HOPOPT_OPT_W		0x29

#define HOPOPT_OPT_V_HDR_LEN	7
#define HOPOPT_OPT_V_HDR_ALIGN	4

#define HOPOPT_OPT_W_HDR_LEN	12
#define HOPOPT_OPT_W_HDR_ALIGN	8

/*
 * Configure this socket to send packets with a hop-by-hop extension header,
 * containing the specified number of hop-by-hop options.
 *
 * Reference:  Section 22.1 of RFC 3542
 * Reference:  Appendix B of RFC 2460
 */
int
ipv6_set_hopopts_ext_hdr(int fd, int num_hdr_opts)
{
	void          *extbuf;
	socklen_t     extlen;
	int           currentlen;
	void          *databuf;
	int           offset;
	uint8_t       value1;
	uint16_t      value2;
	uint32_t      value4;
	uint64_t      value8;
	int           rc;
	int           num_opts;

	if (num_hdr_opts < 0) {
		return (-2);
	}

	/*
	 * Compute the length of the extension header, including
	 * any contained options.
	 * Note:  This could be simplified by using defines; but,
	 * the code is educational, so it remains pedantic.
	 */
	currentlen = inet6_opt_init(NULL, 0);
	if (currentlen == -1) {
		return (-3);
	}

	for (num_opts=0; num_opts < num_hdr_opts; num_opts += 2) {
		currentlen = inet6_opt_append(NULL, 0, currentlen, HOPOPT_OPT_X,
		    HOPOPT_OPT_X_HDR_LEN, HOPOPT_OPT_X_HDR_ALIGN, NULL);
		if (currentlen == -1) {
			return (-4);
		}

		/*
		 * Terminate early if an odd number of options were requested,
		 * and we just computed the length for the last option.
		 */
		if ((num_hdr_opts - num_opts) < 2) {
			break;
		}

		currentlen = inet6_opt_append(NULL, 0, currentlen, HOPOPT_OPT_Y,
		    HOPOPT_OPT_Y_HDR_LEN, HOPOPT_OPT_Y_HDR_ALIGN, NULL);
		if (currentlen == -1) {
			return (-5);
		}
	}

	currentlen = inet6_opt_finish(NULL, 0, currentlen);
	if (currentlen == -1) {
		return (-6);
	}
	extlen = currentlen;

	/* Allocate a buffer in which to build the extension headers */
	extbuf = malloc(extlen);
	if (extbuf == NULL) {
		return (-7);
	}

	/*
	 * Construct the extension header.  Add 0 or more options as
	 * requested by the caller.
	 */
	currentlen = inet6_opt_init(extbuf, extlen);
	if (currentlen == -1) {
		rc = -8;
		goto error_exit;
	}

	for (num_opts=0; num_opts < num_hdr_opts; num_opts += 2) {
		currentlen = inet6_opt_append(extbuf, extlen, currentlen,
		    HOPOPT_OPT_X, HOPOPT_OPT_X_HDR_LEN, HOPOPT_OPT_X_HDR_ALIGN,
		    &databuf);
		if (currentlen == -1) {
			rc = -9;
			goto error_exit;
		}

		/* Insert value 0x12345678 into 4-octet field */
		offset = 0;
		value4 = htonl(0x12345678);
		offset = inet6_opt_set_val(databuf, offset,
		    &value4, sizeof(value4));
		/* Insert value 0x1122334455667788 into 8-octet field */
		value8 = htonll(0x1122334455667788);
		offset = inet6_opt_set_val(databuf, offset,
		    &value8, sizeof(value8));

		/*
		 * Terminate early if an odd number of options were requested,
		 * and we just inserted the last option.
		 */
		if ((num_hdr_opts - num_opts) < 2) {
			break;
		}

		currentlen = inet6_opt_append(extbuf, extlen, currentlen,
		    HOPOPT_OPT_Y, HOPOPT_OPT_Y_HDR_LEN, HOPOPT_OPT_Y_HDR_ALIGN,
		    &databuf);
		if (currentlen == -1) {
			rc = -10;
			goto error_exit;
		}

		/* Insert value 0xb1 for 1-octet field */
		offset = 0;
		value1 = 0xb1;
		offset = inet6_opt_set_val(databuf, offset,
		    &value1, sizeof(value1));
		/* Insert value 0x1331 for 2-octet field */
		value2 = htons(0x1331);
		offset = inet6_opt_set_val(databuf, offset,
		    &value2, sizeof(value2));
		/* Insert value 0xc1c2c3c4 for 4-octet field */
		value4 = htonl(0xc1c2c3c4);
		offset = inet6_opt_set_val(databuf, offset,
		    &value4, sizeof(value4));
	}

	currentlen = inet6_opt_finish(extbuf, extlen, currentlen);
	if (currentlen == -1) {
		rc = -11;
		goto error_exit;
	}

	/*
	 * extbuf is now completely formatted, and extlen contains
	 * the formatted length.
	 *
	 * Call setsockopt() to apply these IPv6 extension headers
	 * as a sticky option to this socket.  This means the extension
	 * headers will be inserted in all subsequent IPv6 packets
	 * sent on this socket, until explicitly turned off with another
	 * setsockopt() call.
	 */
	rc = setsockopt(fd, IPPROTO_IPV6, IPV6_HOPOPTS, extbuf, extlen);
	if (rc) {
		free(extbuf);
		err_sys("ipv6_set_hopopts_ext_hdr():  setsockopt() error");
	}

error_exit:
	free(extbuf);

	return (rc);
}

/*
 * Configure this socket to send packets with a hop-by-hop extension header,
 * containing the specified number of hop-by-hop options.
 *
 * Reference:  Section 4.6 of RFC 8200
 * Reference:  Appendix A of RFC 8200
 * Reference:  Section 22.1 of RFC 3542
 * Reference:  Appendix B of RFC 2460
 */
int
ipv6_set_dstopts_ext_hdr(int fd, int num_hdr_opts)
{
	void          *extbuf;
	socklen_t     extlen;
	int           currentlen;
	void          *databuf;
	int           offset;
	uint8_t       value1;
	uint16_t      value2;
	uint32_t      value4;
	uint64_t      value8;
	int           rc;
	int           num_opts;

	if (num_hdr_opts < 0) {
		return (-2);
	}

	/*
	 * Compute the length of the extension header, including
	 * any contained options.
	 * Note:  Algorithm could be simplified by using defines, but
	 * this code is educational.
	 */
	currentlen = inet6_opt_init(NULL, 0);
	if (currentlen == -1) {
		return (-3);
	}

	for (num_opts=0; num_opts < num_hdr_opts; num_opts += 2) {
		currentlen = inet6_opt_append(NULL, 0, currentlen, HOPOPT_OPT_V,
		    HOPOPT_OPT_V_HDR_LEN, HOPOPT_OPT_V_HDR_ALIGN, NULL);
		if (currentlen == -1) {
			return (-4);
		}

		/*
		 * Terminate early if an odd number of options were requested,
		 * and we just computed the length for the last option.
		 */
		if ((num_hdr_opts - num_opts) < 2) {
			break;
		}

		currentlen = inet6_opt_append(NULL, 0, currentlen, HOPOPT_OPT_W,
		    HOPOPT_OPT_W_HDR_LEN, HOPOPT_OPT_W_HDR_ALIGN, NULL);
		if (currentlen == -1) {
			return (-5);
		}
	}

	currentlen = inet6_opt_finish(NULL, 0, currentlen);
	if (currentlen == -1) {
		return (-6);
	}
	extlen = currentlen;

	/* Allocate a buffer in which to build the extension headers */
	extbuf = malloc(extlen);
	if (extbuf == NULL) {
		return (-7);
	}

	/*
	 * Construct the extension header.  Add 0 or more options as
	 * requested by the caller.
	 */
	currentlen = inet6_opt_init(extbuf, extlen);
	if (currentlen == -1) {
		rc = -8;
		goto error_exit;
	}

	for (num_opts=0; num_opts < num_hdr_opts; num_opts += 2) {
		currentlen = inet6_opt_append(extbuf, extlen, currentlen,
		    HOPOPT_OPT_V, HOPOPT_OPT_V_HDR_LEN, HOPOPT_OPT_V_HDR_ALIGN,
		    &databuf);
		if (currentlen == -1) {
			rc = -9;
			goto error_exit;
		}

		/* Insert value 0xfb for 1-octet field */
		offset = 0;
		value1 = 0xfb;
		offset = inet6_opt_set_val(databuf, offset,
		    &value1, sizeof(value1));
		/* Insert value 0xc569 for 2-octet field */
		value2 = htons(0xc569);
		offset = inet6_opt_set_val(databuf, offset,
		    &value2, sizeof(value2));
		/* Insert value 0x595a5b5c for 4-octet field */
		value4 = htonl(0x595a5b5c);
		offset = inet6_opt_set_val(databuf, offset,
		    &value4, sizeof(value4));

		/*
		 * Terminate early if an odd number of options were requested,
		 * and we just inserted the last option.
		 */
		if ((num_hdr_opts - num_opts) < 2) {
			break;
		}

		currentlen = inet6_opt_append(extbuf, extlen, currentlen,
		    HOPOPT_OPT_W, HOPOPT_OPT_W_HDR_LEN, HOPOPT_OPT_W_HDR_ALIGN,
		    &databuf);
		if (currentlen == -1) {
			rc = -10;
			goto error_exit;
		}

		/* Insert value 0x87654321 into 4-octet field */
		offset = 0;
		value4 = htonl(0x87654321);
		offset = inet6_opt_set_val(databuf, offset,
		    &value4, sizeof(value4));
		/* Insert value 0x8171615141312111 into 8-octet field */
		value8 = htonll(0x8171615141312111);
		offset = inet6_opt_set_val(databuf, offset,
		    &value8, sizeof(value8));
	}

	currentlen = inet6_opt_finish(extbuf, extlen, currentlen);
	if (currentlen == -1) {
		rc = -11;
		goto error_exit;
	}

	/*
	 * extbuf is now completely formatted, and extlen contains
	 * the formatted length.
	 *
	 * Call setsockopt() to apply these IPv6 extension headers
	 * as a sticky option to this socket.  This means the extension
	 * headers will be inserted in all subsequent IPv6 packets
	 * sent on this socket, until explicitly turned off with another
	 * setsockopt() call.
	 */
	rc = setsockopt(fd, IPPROTO_IPV6, IPV6_DSTOPTS, extbuf, extlen);
	if (rc) {
		free(extbuf);
		err_sys("ipv6_set_dstopts_ext_hdr():  setsockopt() error");
	}

error_exit:
	free(extbuf);

	return (rc);
}

/*
 *
 *
 * This function looks like it should work.  The routing header option
 * data is laid out in the buffer as shown in RFC 2292, section 8.9.
 * The setsockopt() call does not return an error.  However, a subsequent
 * sendto() or write() call fails with an "invalid argument" error, and
 * does not send a packet.
 *
 * Research revealed a number of security advisories about IPv6 type 0
 * route headers.  "[They] can be used to mount DoS attacks against hosts
 * and networks.  This is a design flaw in IPv6 and not a bug in OpenBSD."
 * see https://www.openbsd.org/errata40.html#012_route6
 * - 012: SECURITY FIX: April 23, 2007
 *
 * It is unclear if FreeBSD was ever able to transmit IPv6 packets containing
 * type 0 route headers.  FreeBSD 11.2 is evidently not able to do so.
 *
 * https://www.kb.cert.org/vuls/id/267289/
 * https://www.freebsd.org/security/advisories/FreeBSD-SA-07:03.ipv6.asc
 * http://www.secdev.org/conf/IPv6_RH_security-csw07.pdf
 * 
 * RFC 5095 (proposed standard) deprecates IPv6 type 0 routing header
 * 
 * RFC 3775 defines a mobile IPv6 type 2 routing header.  It is not
 * supported by FreeBSD 11.2, which only supports the construction of
 * IPv6 type 0 routing headers.
 */
int
ipv6_set_rthdrs_ext_hdr(int fd, int num_hdr_routes)
{
	char                  *extbuf;
	socklen_t             extlen;
	int                   rc;
	int                   num_routes;
	struct sockaddr_in6   routeaddr6;
	char                  inaddr_buf[INET6_ADDRSTRLEN];

	/* 0 should be OK, but setsockopt() fails with invalid argument */
	if ((num_hdr_routes < 1) || (num_hdr_routes > 127)) {
		return (-2);
	}

	/*
	 * Converts an IPv6 address from a string ("presentation format")
	 * to network format.  The address should really come from one of
	 * the command line options or the server address.
	 */
	if (inet_pton(AF_INET6, "2001:558:4000:b0::1", inaddr_buf) == 1) {
		/* Converted IPv6 address */
		memcpy(&routeaddr6.sin6_addr, inaddr_buf,
		    sizeof(struct in6_addr));
	}

// Fixme
num_hdr_routes = 1;

	extlen = inet6_rth_space(IPV6_RTHDR_TYPE_0, num_hdr_routes);
	if (extlen == 0) {
		return (-3);
	}
	extbuf = malloc(extlen);
	if (extbuf == NULL) {
		return (-4);
	}

	extbuf =
	    inet6_rth_init(extbuf, extlen, IPV6_RTHDR_TYPE_0, num_hdr_routes);
	if (extbuf == NULL) {
		rc = -5;
		goto error_exit;
	}

	for (num_routes=0; num_routes < num_hdr_routes; num_routes++) {
		rc = inet6_rth_add(extbuf, &routeaddr6.sin6_addr);
		if (rc != 0) {
			rc = -6;
			goto error_exit;
		}
	}

	/*
	 * Note:  Returns an error if num_hdr_routes == 0
	 * Note:  Returns an error if rth0->ip6r0_type != 0
	 */
	rc = setsockopt(fd, IPPROTO_IPV6, IPV6_RTHDR, extbuf, extlen);
	if (rc) {
		printf("ipv6_set_rthdrs_ext_hdr():  rc = %d\n", rc);
		printf("ipv6_set_rthdrs_ext_hdr():  extlen %d\n", extlen);
		err_sys("setsockopt() error");
	}

error_exit:
	//printf("ipv6_set_rthdrs_ext_hdr() error exit:  rc = %d\n", rc);
	free(extbuf);

	return (rc);
}

