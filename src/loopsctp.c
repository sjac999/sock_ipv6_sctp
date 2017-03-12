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

#include "sock.h"

/*
 * Invoked for an SCTP server when sourcesink is NOT set by the -i option.
 * 
 * Copies everything from stdin to "sockfd",
 * and everything from "sockfd" to stdout.
 */
void loop_sctp(int sockfd)
{
	int		maxfdp1, nread, ntowrite, stdineof, flags;
	fd_set		rset;

	if (pauseinit) {
		sleep_us(pauseinit * 1000);	/* intended for server */
	}
  
	flags = 0;
	stdineof = 0;
	FD_ZERO(&rset);
	maxfdp1 = sockfd + 1;	/* check descriptors [0..sockfd] */
  
	for ( ; ; ) {
		if (stdineof == 0) {
			FD_SET(STDIN_FILENO, &rset);
		}
		FD_SET(sockfd, &rset);
      
		if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0) {
			err_sys("select error");
		}
      
		if (FD_ISSET(STDIN_FILENO, &rset)) {
			/* data to read on stdin */
			if ( (nread = read(STDIN_FILENO, rbuf, readlen)) < 0) {
				err_sys("read error from stdin");
			} else if (nread == 0) {
				/* EOF on stdin */
				if (halfclose) {
					if (shutdown(sockfd, SHUT_WR) < 0) {
						err_sys("shutdown() error");
					}
					FD_CLR(STDIN_FILENO, &rset);
					/* don't read stdin anymore */
					stdineof = 1;
					continue;	/* back to select() */
				}
				/* default: stdin EOF -> done */
				break;
			}
	  
			if (crlf) {
				ntowrite = crlf_add(wbuf, writelen, rbuf,
				    nread);
				if (dowrite(sockfd, wbuf, ntowrite) !=
								 ntowrite) {
					err_sys("write error");
				}
			} else {
				if (dowrite(sockfd, rbuf, nread) != nread) {
					err_sys("write error");
				}
			}
		}
      
		/*
		 * If there is data to read from the socket.
		 */
		if (FD_ISSET(sockfd, &rset)) {
			/* msgpeek = 0 or MSG_PEEK */
			flags = msgpeek;
oncemore:
			if ( (nread = recv(sockfd, rbuf, readlen, flags)) < 0) {
				err_sys("recv error");
			} else if (nread == 0) {
				if (verbose) {
					fprintf(stderr,
					    "connection closed by peer\n");
				}
				/* EOF, terminate */
				break;
			}

			if (crlf) {
				ntowrite = crlf_strip(wbuf, writelen, rbuf,
				    nread);
				if (writen(STDOUT_FILENO, wbuf, ntowrite) !=
								  ntowrite) {
					err_sys("writen error to stdout");
				}
			} else {
				if (writen(STDOUT_FILENO, rbuf, nread) !=
								     nread) {
					err_sys("writen error to stdout");
				}
			}
			if (flags != 0) {
				/* no infinite loop */
				flags = 0;
				/* read the message again */
				goto oncemore;
			}
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
