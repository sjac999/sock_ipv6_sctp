/* -*- c-basic-offset: 8; -*-
 *
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include "sock.h"

/*
 * Try to convert the host name as an IPv4 dotted-decimal number
 * or an IPv6 address.
 */
int convert_host_address(char *host)
{
	struct in_addr		inaddr;
	char			inaddr_buf[INET6_ADDRSTRLEN];

	if (AF_INET == af_46) {
		if (inet_pton(AF_INET, host, &inaddr) == 1) {
			/* IPv4 dotted-decimal */
			servaddr4.sin_addr = inaddr;

			return (1);
		}
	} else {
		if (inet_pton(AF_INET6, host, inaddr_buf) == 1) {
			/* IPv6 address */
			memcpy(&servaddr6.sin6_addr, inaddr_buf,
			    sizeof(struct in6_addr));

			return (1);
		}
	}

	return (0);
}

/*
 * Try to convert the host name as a host name string.
 */
int convert_host_name(char *host)
{
	struct hostent		*hp;

	if (AF_INET == af_46) {
		if ( (hp = gethostbyname2(host, AF_INET)) != NULL) {
			/* IPv4 address */
			memcpy(&servaddr4.sin_addr, hp->h_addr, hp->h_length);

			return (1);
		}
	} else {
		/*
		 * Fixme:  This doesn't work on FreeBSD 8.4.
		 * Only an IPv4 address is returned.
		 * getaddrinfo() doesn't work either.
		 */
		if ( (hp = gethostbyname2(host, AF_INET6)) != NULL) {
			/* IPv6 address */
			memcpy(&servaddr6.sin6_addr, hp->h_addr, hp->h_length);

			return (1);
		}
	}

	return (0);
}

int cliopen(char *host, char *port)
{
	int			fd, i, on;
	char			*protocol;
	char			inaddr_buf[INET6_ADDRSTRLEN];
	struct servent		*sp;
	socklen_t		socklen;
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
  
	/* initialize socket address structure */
	bzero(&servaddr4, sizeof(servaddr4));
	servaddr4.sin_family = AF_INET;

	bzero(&servaddr6, sizeof(servaddr6));
	servaddr6.sin6_family = AF_INET6;
  
	/* see if "port" is a service name or number */
	if ( (i = atoi(port)) == 0) {
		if ( (sp = getservbyname(port, protocol)) == NULL)
			err_quit("getservbyname() error for: %s/%s",
			    port, protocol);
		servaddr4.sin_port  = sp->s_port;
		servaddr6.sin6_port = sp->s_port;
	} else {
		servaddr4.sin_port  = htons(i);
		servaddr6.sin6_port = htons(i);
	}
  
	/*
	 * First try to convert the host name as an IPv4 dotted-decimal number
	 * or an IPv6 address.  Only if that fails do we try to convert the
	 * host name as a host name string.
	 */
	if (convert_host_address(host) != 1) {
		if (convert_host_name(host) != 1) {
			err_quit("invalid hostname: %s", host);
		}
	}

	if ( (fd = socket(af_46, sock_type, sock_prot)) < 0) {
		err_sys("socket() error");
	}
	if (reuseaddr) {
		on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)) < 0)
			err_sys("setsockopt of SO_REUSEADDR error");
	}

#ifdef	SO_REUSEPORT
	if (reuseport) {
		on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof (on)) < 0)
			err_sys("setsockopt of SO_REUSEPORT error");
	}
#endif

	/*
	 * User can specify port number for client to bind.  Only real use
	 * is to see a TCP connection initiated by both ends at the same time.
	 * Also, if UDP is being used, we specifically call bind() to assign
	 * an ephemeral port to the socket.
	 * Also, for experimentation, client can also set local IP address
	 * (and port) using -l option.  Allow localip[] to be set but bindport
	 * to be 0.
	 * Fixme:  What about SCTP?
	 */
	if (bindport != 0 || localip[0] != 0 || l4_prot == L4_PROT_UDP) {
		if (af_46 == AF_INET) {
			bzero(&cliaddr4, sizeof(cliaddr4));
			cliaddr4.sin_family      = AF_INET;
			/* can be 0 */
			cliaddr4.sin_port        = htons(bindport);
			if (localip[0] != 0) {
				if (inet_aton(localip, &cliaddr4.sin_addr) == 0)
					err_quit("invalid IP address: %s",
					    localip);
			} else {
				/* wildcard */
				cliaddr4.sin_addr.s_addr = htonl(INADDR_ANY);
			}
			if (bind(fd, (struct sockaddr *) &cliaddr4,
				    sizeof(cliaddr4)) < 0) {
				err_sys("bind() error");
			}
		} else {
			bzero(&cliaddr6, sizeof(cliaddr6));
			cliaddr6.sin6_len    = sizeof(struct sockaddr_in6);
			cliaddr6.sin6_family = AF_INET6;
			/* can be 0 */
			cliaddr6.sin6_port   = htons(bindport);

			/* Fixme:  localip not implemented for IPv6 */
			cliaddr6.sin6_addr = in6addr_any;

			if (bind(fd, (struct sockaddr *) &cliaddr6,
				    sizeof(cliaddr6)) < 0) {
				err_sys("bind() error");
			}
		}
	}

	/* Fixme:  Does not work */
	//join_mcast_client(fd, &cliaddr4, &cliaddr6, &servaddr4, &servaddr6);

	/*
	 * Need to allocate buffers before connect(), since they can affect
	 * TCP options (window scale, etc.).
	 */
	
	buffers(fd);
	sockopts(fd, 0);	/* may also want to set SO_DEBUG */
	
	/*
	 * Connect to the server.  Required for TCP, optional for UDP.
	 * Fixme:  Don't know about SCTP
	 */
	if (l4_prot == L4_PROT_TCP || connectudp) {
		for ( ; ; ) {
			if (AF_INET == af_46) {
				if (connect(fd, (struct sockaddr *) &servaddr4,
				    sizeof(servaddr4)) == 0)
				break;		/* all OK */
			} else {
				servaddr6.sin6_len =
				    sizeof(struct sockaddr_in6);
				servaddr6.sin6_family = AF_INET6;
				if (connect(fd, (struct sockaddr *) &servaddr6,
				    sizeof(servaddr6)) == 0)
				break;		/* all OK */
			}
			if (errno == EINTR)	/* can happen with SIGIO */
				continue;
			if (errno == EISCONN)	/* can happen with SIGIO */
				break;
			err_sys("connect() error");
		}
	}
  
	if (verbose) {
		/* Call getsockname() to find local address bound to socket:
		   TCP ephemeral port was assigned by connect() or bind();
		   UDP ephemeral port was assigned by bind(). */
		if (AF_INET == af_46) {
			socklen = sizeof(cliaddr4);
			if (getsockname(fd,
			    (struct sockaddr *) &cliaddr4, &socklen) < 0) {
				err_sys("getsockname() error");
			}
			/* Can't do one fprintf() since inet_ntoa() stores
			   the result in a static location. */
			fprintf(stderr, "connected on %s.%d ",
			    INET_NTOA(cliaddr4.sin_addr),
			    ntohs(cliaddr4.sin_port));
			fprintf(stderr, "to %s.%d\n",
			    INET_NTOA(servaddr4.sin_addr),
			    ntohs(servaddr4.sin_port));
		} else {
			socklen = sizeof(cliaddr6);
			if (getsockname(fd,
			    (struct sockaddr *) &cliaddr6, &socklen) < 0) {
				err_sys("getsockname() error");
			}

			inet_ntop(AF_INET6,
			    &cliaddr6.sin6_addr.__u6_addr.__u6_addr8,
			    inaddr_buf, sizeof(inaddr_buf));
			fprintf(stderr, "connected on %s.%d ",
			    inaddr_buf, ntohs(cliaddr6.sin6_port));
			inet_ntop(AF_INET6,
			    &servaddr6.sin6_addr.__u6_addr.__u6_addr8,
			    inaddr_buf, sizeof(inaddr_buf));
			fprintf(stderr, "to %s.%d\n",
			    inaddr_buf, ntohs(servaddr6.sin6_port));
		}
	}
	
	sockopts(fd, 1);	/* some options get set after connect() */
	
	return(fd);
}
