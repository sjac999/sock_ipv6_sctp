/* -*- c-basic-offset: 8; -*-
 *
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Copyright (c) 2017 by Steven A. Jacobson.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The authors make no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include <stdio.h>
#include "sock.h"

/*
 * Invoked for an SCTP server when sourcesink is set by the -i option.
 */
void
sink_sctp(int sockfd)
{
	int		n, flags;

	if (pauseinit) {
		sleep_us(pauseinit * 1000);
	}

	/*
	 * Read until peer closes connection; -n option ignored.
	 */
	for ( ; ; ) {
		/* msgpeek = 0 or MSG_PEEK */
		flags = msgpeek;
oncemore:
		if ( (n = recv(sockfd, rbuf, readlen, flags)) < 0) {
			err_sys("recv error");
		} else if (n == 0) {
			if (verbose)
				fprintf(stderr, "connection closed by peer\n");
			break;
			
#ifdef notdef
		/* The following not possible with TCP */
		/* What about SCTP? */
		} else if (n != readlen)
			err_quit("read returned %d, expected %d", n, readlen);
#else
		}
#endif
	
		if (verbose) {
			fprintf(stderr, "received %d bytes%s\n", n,
				(flags == MSG_PEEK) ? " (MSG_PEEK)" : "");
		}
		if (pauserw) {
			sleep_us(pauserw * 1000);
		}
		if (flags != 0) {
			/* no infinite loop */
			flags = 0;
			/* read the message again */
			goto oncemore;
		}
	}

	if (pauseclose) {
		if (verbose) {
			fprintf(stderr, "pausing before close\n");
		}
		sleep_us(pauseclose * 1000);
 	}

	/* Relevant to SCTP? */
	if (close(sockfd) < 0) {
		err_sys("close error");
	}
}

