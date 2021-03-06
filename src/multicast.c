/* -*- c-basic-offset: 8; -*-
 *
 * Copyright (c) 1993 W. Richard Stevens.  All rights reserved.
 * Permission to use or modify this software and its documentation only for
 * educational purposes and without fee is hereby granted, provided that
 * the above copyright notice appear in all copies.  The author makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

#include	"sock.h"

/*
 * Works for IPv4 and IPv6.
 * For IPv4, the server is able to join a multicast group such as 224.0.0.1.
 * The client is able to connect to the same multicast address and port
 * assigned to the server.  The client can then send data, which the server 
 * receives.
 * For IPv6, the server is able to join a multicast group of the form ff02::n.
 * The client is never able to connect to that address/port, however.
 */
void
join_mcast_server(int fd, struct sockaddr_in *sin, struct sockaddr_in6 *sin6)
{
#ifdef	IP_ADD_MEMBERSHIP	/* only include if host supports mcasting */
	u_long		 inaddr;
	struct ip_mreq	 mreq;
	struct ipv6_mreq mreq6;

	if (af_46 == AF_INET) {
		inaddr = sin->sin_addr.s_addr;
		/*
		 * FreeBSD 8.4:  IN_MULTICAST() test fails in the original code
		 * because it was testing inaddr with the wrong endianness.
		 * Fixed in this version of the code, at least for FreeBSD.
		 */
		inaddr = ntohl(inaddr);

		if (IN_MULTICAST(inaddr) == 0) {
			return;			/* not a multicast address */
		}
	} else {
		if (IN6_IS_ADDR_MULTICAST(&(sin6->sin6_addr)) == 0) {
			return;			/* not a multicast address */
		}
	}

	if (af_46 == AF_INET) {
		mreq.imr_multiaddr.s_addr = sin->sin_addr.s_addr;
		/* need way to change */
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
		    sizeof(mreq)) == -1 ) {
			err_sys("IPv4 IP_ADD_MEMBERSHIP error");
		}
	} else {
		memset((char *)&mreq6, 0, sizeof(mreq6));
// Fixme
// Fixme
// Fixme
		/* Pick an appropriate interface */
		mreq6.ipv6mr_interface = 0;
		mreq6.ipv6mr_multiaddr = sin6->sin6_addr;
// Fixme
// Fixme
// Fixme
		//mreq6.ipv6mr_interface = sin6->sin6_scope_id;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6,
		    sizeof(mreq6)) == -1 ) {
			err_sys("IPv6 IPV6_JOIN_GROUP error");
		}
	}
	if (verbose) {
		fprintf(stderr, "multicast group joined\n");
	}
#endif	/* IP_ADD_MEMBERSHIP */
}

/*
 * Added for IPv6.  Fixme:  Does not work at present.
 * The client is currently unable to connect to the multicast address/port
 * which was joined by the server.  A function similar to this may be needed
 * to permit that connection to be made.
 */
void
join_mcast_client(int fd,
		  struct sockaddr_in *cli_sin, struct sockaddr_in6 *cli_sin6,
		  struct sockaddr_in *serv_sin, struct sockaddr_in6 *serv_sin6)
{
#ifdef	IP_ADD_MEMBERSHIP	/* only include if host supports mcasting */
	struct ipv6_mreq mreq6;

	if (af_46 == AF_INET6) {
		if (IN6_IS_ADDR_MULTICAST(&(serv_sin6->sin6_addr)) == 0) {
			return;			/* not a multicast address */
		}
	}

	if (af_46 == AF_INET6) {
		memset((char *)&mreq6, 0, sizeof(mreq6));
// Fixme
// Fixme
// Fixme
		/* Pick an appropriate interface */
		mreq6.ipv6mr_interface = 0;
		mreq6.ipv6mr_multiaddr = serv_sin6->sin6_addr;
// Fixme
// Fixme
// Fixme
		//mreq6.ipv6mr_interface = serv_sin6->sin6_scope_id;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6,
		    sizeof(mreq6)) == -1 ) {
			err_sys("IPv6 IPV6_JOIN_GROUP error");
		}
	}

	if (verbose) {
		fprintf(stderr, "multicast group joined\n");
	}
#endif	/* IP_ADD_MEMBERSHIP */
}
