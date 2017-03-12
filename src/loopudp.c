/* -*- c-basic-offset: 8; -*-
 *
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sock.h"

/* Copy everything from stdin to "sockfd",
 * and everything from "sockfd" to stdout. */

void
loop_udp(int sockfd)
{
	int maxfdp1, nread, ntowrite, stdineof, clilen, flags;
	socklen_t		servlen;
	fd_set			rset;
	struct sockaddr_in	cliaddr4;	/* for IPv4 UDP server */
	struct sockaddr_in6	cliaddr6;	/* for IPv6 UDP server */
	/*
	 * The original local variable servaddr, and later servaddr4 and
	 * servaddr6, were not initialized before use.  Using the initialized
	 * global sockaddr structs allows the sendto() code, below, to work
	 * correctly.  This was a problem with the original IPv4 code, and
	 * later the IPv6 code.
	 */
	//struct sockaddr_in	servaddr4;	/* for IPv4 UDP client */
	//struct sockaddr_in6	servaddr6;	/* for IPv6 UDP client */
	char			inaddr_buf[INET6_ADDRSTRLEN];
	
	struct iovec		iov[1];
	struct msghdr		msg;
#ifdef	HAVE_MSGHDR_MSG_CONTROL	  	
#ifdef	IP_RECVDSTADDR		/* 4.3BSD Reno and later */
	static struct cmsghdr  *cmptr = NULL;	/* malloc'ed */
	struct in_addr		dstinaddr;	/* for UDP server */
#define	CONTROLLEN	(sizeof(struct cmsghdr) + sizeof(struct in_addr))
#endif	/* IP_RECVDSTADDR */
	
#endif	/* HAVE_MSGHDR_MSG_CONTROL */
	
	if (pauseinit)
		sleep_us(pauseinit*1000);	/* intended for server */
	
	flags = 0;
	stdineof = 0;
	FD_ZERO(&rset);
	maxfdp1 = sockfd + 1;	/* check descriptors [0..sockfd] */
	
	/* If UDP client issues connect(), recv() and write() are used.
	   Server is harder since cannot issue connect().  We use recvfrom()
	   or recvmsg(), depending on OS. */
	
	for ( ; ; ) {
		if (stdineof == 0)
			FD_SET(STDIN_FILENO, &rset);
		FD_SET(sockfd, &rset);
		
		if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0)
			err_sys("select error");
		
		if (FD_ISSET(STDIN_FILENO, &rset)) {
			/* data to read on stdin */
			if ( (nread = read(STDIN_FILENO, rbuf, readlen)) < 0)
				err_sys("read error from stdin");
			else if (nread == 0) {
				/* EOF on stdin */
				if (halfclose) {
					if (shutdown(sockfd, SHUT_WR) < 0)
						err_sys("shutdown() error");
					
					FD_CLR(STDIN_FILENO, &rset);
					/* don't read stdin anymore */
					stdineof = 1;
					/* back to select() */
					continue;
				}
				break;		/* default: stdin EOF -> done */
			}
	  
			if (crlf) {
				ntowrite = crlf_add(wbuf, writelen, rbuf, nread);
				if (connectudp) {
					if (write(sockfd, wbuf, ntowrite) !=
					    ntowrite) {
						err_sys("write error");
					}
				} else {
					if (af_46 == AF_INET) {
						if (sendto(sockfd, wbuf, ntowrite, 0,
						    (struct sockaddr *)&servaddr4,
						    sizeof(servaddr4)) != ntowrite) {
							err_sys("sendto error");
						}
					} else {
						if (sendto(sockfd, wbuf, ntowrite, 0,
						    (struct sockaddr *)&servaddr6,
						    sizeof(servaddr6)) != ntowrite) {
							err_sys("sendto error");
						}
					}
				}
			} else {
				if (connectudp) {
					if (write(sockfd, rbuf, nread) != nread)
						err_sys("write error");
				} else {
					if (af_46 == AF_INET) {
						if (sendto(sockfd, rbuf, nread, 0,
						    (struct sockaddr *)&servaddr4,
						    sizeof(servaddr4)) != nread) {
							err_sys("sendto error");
						}
					} else {
						if (sendto(sockfd, rbuf, nread, 0,
						    (struct sockaddr *)&servaddr6,
						    sizeof(servaddr6)) != nread) {
							err_sys("sendto error");
						}
					}
				}
			}
		}
      
		if (FD_ISSET(sockfd, &rset)) {
			/* data to read from socket */
			if (server) {
				if (af_46 == AF_INET) {
					clilen = sizeof(cliaddr4);
				} else {
					clilen = sizeof(cliaddr6);
				}

#ifndef	MSG_TRUNC /* vanilla BSD sockets */

				/* Fixme:  Not ported for IPv6 */
				/* Not compiled in for FreeBSD 8.4 */
				nread = recvfrom(sockfd, rbuf, readlen, 0,
				    (struct sockaddr *) &cliaddr4, &clilen);

#else	/* 4.3BSD Reno and later; use recvmsg() to get at MSG_TRUNC flag */
	/* Also lets us get at control information (destination address) */

				/* FreeBSD 8.4 */
				iov[0].iov_base = rbuf;
				iov[0].iov_len  = readlen;
				msg.msg_iov     = iov;
				msg.msg_iovlen  = 1;
				if (af_46 == AF_INET) {
					msg.msg_name = (caddr_t) &cliaddr4;
				} else {
					msg.msg_name = (caddr_t) &cliaddr6;
				}
				msg.msg_namelen = clilen;

#ifdef	HAVE_MSGHDR_MSG_CONTROL
#ifdef	IP_RECVDSTADDR
				/* FreeBSD 8.4 */
				if (cmptr == NULL && (cmptr = malloc(CONTROLLEN)) == NULL)
					err_sys("malloc error for control buffer");

				/* for dest address */
				msg.msg_control      = (caddr_t) cmptr;
				msg.msg_controllen   = CONTROLLEN;
#else
				/* Not used for FreeBSD 8.4 */
				/* no ancillary data */
				msg.msg_control      = (caddr_t) 0;
				msg.msg_controllen   = 0;
#endif	/* IP_RECVDSTADDR */
#endif
#ifdef HAVE_MSGHDR_MSG_FLAGS
				/* FreeBSD 8.4 */
				/* flags returned here */
				msg.msg_flags        = 0;
#endif				
				nread = recvmsg(sockfd, &msg, 0);
#endif	/* MSG_TRUNC */
				if (nread < 0)
					err_sys("datagram receive error");
				
				if (verbose) {
					if (af_46 == AF_INET) {
						printf("from %s",
						    INET_NTOA(cliaddr4.sin_addr));
					} else {
			                        inet_ntop(AF_INET6,
						    &cliaddr6.sin6_addr.
						    __u6_addr.__u6_addr8,
						    inaddr_buf,
						    sizeof(inaddr_buf));
						printf("from %s", inaddr_buf);
					}
#ifdef	HAVE_MSGHDR_MSG_CONTROL
#ifdef	IP_RECVDSTADDR
					/*
					 * Fixme:  not ported for IPv6
					 * Fixme:  recvdstaddr fails (earlier,
					 *   in setsockopt()) for IPv6 under
					 *   FreeBSD 8.4
					 */
					if (recvdstaddr) {
						if (cmptr->cmsg_len != CONTROLLEN)
							err_quit("control length (%d) != %d",
							    cmptr->cmsg_len, CONTROLLEN);
						if (cmptr->cmsg_level != IPPROTO_IP)
							err_quit("control level != IPPROTO_IP");
						if (cmptr->cmsg_type != IP_RECVDSTADDR)
							err_quit("control type != IP_RECVDSTADDR");
						bcopy(CMSG_DATA(cmptr), &dstinaddr,
						    sizeof(struct in_addr));
						bzero(cmptr, CONTROLLEN);
						
						printf(", to %s",
						    INET_NTOA(dstinaddr));
					}
#endif	/* IP_RECVDSTADDR */
#endif	/* HAVE_MSGHDR_MSG_CONTROL */
					printf(": ");
					fflush(stdout);
				}
#ifdef HAVE_MSGHDR_MSG_FLAGS	      
#ifdef MSG_TRUNC
				if (msg.msg_flags & MSG_TRUNC)
					printf("(datagram truncated)\n");
#endif /* MSG_TRUNC */
#endif /* HAVE_MSGHDR_MSG_FLAGS */      
			} else if (connectudp) {
				/* msgpeek = 0 or MSG_PEEK */
				flags = msgpeek;
			oncemore:
				if ( (nread = recv(sockfd, rbuf, readlen, flags)) < 0)
					err_sys("recv error");
				else if (nread == 0) {
					if (verbose)
						fprintf(stderr, "connection closed by peer\n");
					break;		/* EOF, terminate */
				}
	      
			} else {
				/*
				 * Must use recvfrom() for unconnected
				 * UDP client
				 */
				/* Fixme:  not tested on FreeBSD 8.4 */
				if (af_46 == AF_INET) {
					servlen = sizeof(servaddr4);
					nread = recvfrom(sockfd, rbuf, readlen,
					    0, (struct sockaddr *)&servaddr4,
					    &servlen);
				} else {
					servlen = sizeof(servaddr6);
					nread = recvfrom(sockfd, rbuf, readlen,
					    0, (struct sockaddr *)&servaddr6,
					    &servlen);
				}
				if (nread < 0) {
					err_sys("datagram recvfrom() error");
				}
				if (verbose) {
					if (af_46 == AF_INET) {
						printf("from %s",
						    INET_NTOA(servaddr4.sin_addr));
					} else {
			                        inet_ntop(AF_INET6,
						    &servaddr6.sin6_addr.
						    __u6_addr.__u6_addr8,
						    inaddr_buf,
						    sizeof(inaddr_buf));
						printf("from %s", inaddr_buf);
					}
					printf(": ");
					fflush(stdout);
				}
			}
			
			if (crlf) {
				ntowrite = crlf_strip(wbuf, writelen, rbuf, nread);
				if (writen(STDOUT_FILENO, wbuf, ntowrite) != ntowrite)
					err_sys("writen error to stdout");
			} else {
				if (writen(STDOUT_FILENO, rbuf, nread) != nread)
					err_sys("writen error to stdout");
			}
			
			if (flags != 0) {
				flags = 0;		/* no infinite loop */
				goto oncemore;	/* read the message again */
			}
		}
	}
	
	if (pauseclose) {
		if (verbose)
			fprintf(stderr, "pausing before close\n");
		sleep_us(pauseclose*1000);
	}
	
	if (close(sockfd) < 0)
		err_sys("close error");		/* since SO_LINGER may be set */
}
