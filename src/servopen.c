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

int
servopen(char *host, char *port)
{
	int			fd, newfd, i, on, pid;
	char			*protocol;
	struct in_addr		inaddr;
	struct servent		*sp;
	socklen_t		len;
	char			inaddr_buf[INET6_ADDRSTRLEN];
	int			sock_type;
	int			sock_prot;

	switch (l4_prot) {
	case L4_PROT_UDP:
		protocol = "udp";
		sock_type = SOCK_DGRAM;
		sock_prot = 0;
		break;
	case L4_PROT_SCTP:
		protocol = "sctp";
		sock_type = SOCK_STREAM;
		sock_prot = IPPROTO_SCTP;
		break;
	case L4_PROT_TCP:
	default:
		protocol = "tcp";
		sock_type = SOCK_STREAM;
		sock_prot = 0;
		break;
	}

	/* Initialize the socket address structure */
	bzero(&servaddr4, sizeof(servaddr4));
	servaddr4.sin_family  = AF_INET;

	bzero(&servaddr6, sizeof(servaddr6));
	servaddr6.sin6_family = AF_INET6;

	/*
	 * Caller normally wildcards the local Internet address, meaning
	 * a connection will be accepted on any connected interface.
	 * We only allow an IP address for the "host", not a name.
	 */
	if (host == NULL) {
		if (AF_INET == af_46) {
			/* wildcard */
			servaddr4.sin_addr.s_addr = htonl(INADDR_ANY);
		} else {
			/* wildcard */
			servaddr6.sin6_addr = in6addr_any;
		}
	} else {
		if (AF_INET == af_46) {
			if (inet_pton(AF_INET, host, &inaddr) == 0) {
				err_quit("invalid host name for server: %s",
				    host);
			}
			/* IPv4 address */
			servaddr4.sin_addr = inaddr;
		} else {
			if (inet_pton(AF_INET6, host, inaddr_buf) == 0) {
				err_quit("invalid host name for server: %s",
				    host);
			}
			/* IPv6 address */
			memcpy(&servaddr6.sin6_addr, inaddr_buf,
			    sizeof(struct in6_addr));
		}
	}

	/* See if "port" is a service name or number */
	if ( (i = atoi(port)) == 0) {
		if ( (sp = getservbyname(port, protocol)) == NULL)
			err_ret("getservbyname() error for: %s/%s", port,
			    protocol);
		servaddr4.sin_port = sp->s_port;
		servaddr6.sin6_port = sp->s_port;
	} else {
		servaddr4.sin_port = htons(i);
		servaddr6.sin6_port = htons(i);
	}

	if ( (fd = socket(af_46, sock_type, sock_prot)) < 0) {
		err_sys("socket() error");
	}
	if (reuseaddr) {
		on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
			err_sys("setsockopt of SO_REUSEADDR error");
	}

#ifdef	SO_REUSEPORT
	if (reuseport) {
		on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0)
			err_sys("setsockopt of SO_REUSEPORT error");
	}
#endif

	/* Bind our well-known port so the client can connect to us. */
	if (AF_INET == af_46) {
		if (bind(fd, (struct sockaddr *) &servaddr4,
			    sizeof(servaddr4)) < 0) {
			err_sys("can't bind local address");
		}
	} else {
		if (bind(fd, (struct sockaddr *) &servaddr6,
			    sizeof(servaddr6)) < 0) {
			err_sys("can't bind local address");
		}
	}

	join_mcast_server(fd, &servaddr4, &servaddr6);

	// Fixme:  What about SCTP?
	if (l4_prot == L4_PROT_UDP) {
		buffers(fd);

		/* Fixme:  Not ported for IPv6 */
		if (foreignip[0] != 0) {    /* connect to foreignip/port# */
			bzero(&cliaddr4, sizeof(cliaddr4));
			if (inet_aton(foreignip, &cliaddr4.sin_addr) == 0)
				err_quit("invalid IP address: %s", foreignip);
			cliaddr4.sin_family = AF_INET;
			cliaddr4.sin_port   = htons(foreignport);
			/* connect() for datagram socket doesn't appear to allow
			   wildcarding of either IP address or port number */

			if (connect(fd, (struct sockaddr *) &cliaddr4,
			    sizeof(cliaddr4)) < 0)
				err_sys("connect() error");
		}

		sockopts(fd, 1);

		return(fd);		/* nothing else to do */
	}

	buffers(fd);		/* may set receive buffer size; must do here to
				   get correct window advertised on SYN */
	sockopts(fd, 0);	/* only set some socket options for fd */

	if (listen(fd, listenq) < 0) {
		err_sys("listen() error");
	}

	if (pauselisten) {
		/* lets connection queue build up */
		sleep_us(pauselisten*1000);
	}

	if (dofork) {
		/* initialize synchronization primitives */
		TELL_WAIT();
	}

	for ( ; ; ) {
		if (AF_INET == af_46) {
			len = sizeof(cliaddr4);
			if ( (newfd = accept(fd, (struct sockaddr *) &cliaddr4,
				    &len)) < 0) {
				err_sys("accept() error");
			}
		} else {
			len = sizeof(cliaddr6);
			if ( (newfd = accept(fd, (struct sockaddr *) &cliaddr6,
				    &len)) < 0) {
				err_sys("accept() error");
			}
		}

		if (dofork) {
			if ( (pid = fork()) < 0)
				err_sys("fork error");

			if (pid > 0) {
				/* parent closes connected socket */
				close(newfd);
				/* wait for child to output to terminal */
				WAIT_CHILD();
				/* and back to for(;;) for another accept() */
				continue;
			} else {
				/* child closes listening socket */
				close(fd);
			}
		}

		/* child (or iterative server) continues here */
		if (verbose) {
			/*
			 * Call getsockname() to find local address bound
			 * to socket: local internet address is now
			 * determined (if multihomed).
			 */
			if (AF_INET == af_46) {
				len = sizeof(servaddr4);
				if (getsockname(newfd,
				    (struct sockaddr *)&servaddr4, &len) < 0) {
					err_sys("getsockname() error");
				}

				/*
				 * Can't do one fprintf() since inet_ntoa()
				 * stores the result in a static location.
				 */
				fprintf(stderr, "connection on %s.%d ",
				    INET_NTOA(servaddr4.sin_addr),
				    ntohs(servaddr4.sin_port));
				fprintf(stderr, "from %s.%d\n",
				    INET_NTOA(cliaddr4.sin_addr),
				    ntohs(cliaddr4.sin_port));
			} else {
				len = sizeof(servaddr6);
				if (getsockname(newfd,
				    (struct sockaddr *)&servaddr6, &len) < 0) {
					err_sys("getsockname() error");
				}
				inet_ntop(AF_INET6,
				    &servaddr6.sin6_addr.__u6_addr.__u6_addr8,
				    inaddr_buf, sizeof(inaddr_buf));
				fprintf(stderr, "connection on %s.%d ",
				    inaddr_buf, ntohs(servaddr6.sin6_port));
				inet_ntop(AF_INET6,
				    &cliaddr6.sin6_addr.__u6_addr.__u6_addr8,
				    inaddr_buf, sizeof(inaddr_buf));
				fprintf(stderr, "from %s.%d\n",
				    inaddr_buf, ntohs(cliaddr6.sin6_port));
			}
		}

		/* setsockopt() again, in case it didn't propagate
		   from listening socket to connected socket */
		buffers(newfd);

		/* can set all socket options for this socket */
		sockopts(newfd, 1);

		if (dofork) {
			/* tell parent we're done with terminal */
			TELL_PARENT(getppid());
		}

		return(newfd);
	}
}
