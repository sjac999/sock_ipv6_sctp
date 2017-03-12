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
 * Invoked for an SCTP client when sourcesink is set by the -i option.
 */
void
source_sctp(int sockfd)
{
	int		i, n, option;
	socklen_t	optlen;

	/* Fill send buffer with a pattern. */
	pattern(wbuf, writelen);

	if (pauseinit) {
		sleep_us(pauseinit * 1000);
	}

	for (i = 1; i <= nbuf; i++) {
		if ( (n = write(sockfd, wbuf, writelen)) != writelen) {
			if (ignorewerr) {
				err_ret("write returned %d, expected %d",
				    n, writelen);
				/* also call getsockopt() to clear so_error */
				optlen = sizeof(option);
				if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
				    &option, &optlen) < 0) {
					err_sys("SO_ERROR getsockopt error");
				}
			} else {
				err_sys("write returned %d, expected %d", n,
				    writelen);
			}
		} else if (verbose) {
			fprintf(stderr, "wrote %d bytes\n", n);
		}
		if (pauserw) {
			sleep_us(pauserw * 1000);
		}
	}

	if (pauseclose) {
		if (verbose) {
			fprintf(stderr, "pausing before close\n");
		}
		sleep_us(pauseclose * 1000);
	}
	if (close(sockfd) < 0) {
		err_sys("close error");
	}
}

