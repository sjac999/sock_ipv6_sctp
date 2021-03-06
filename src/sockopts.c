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
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "sock.h"
#include <fcntl.h>
#include <sys/ioctl.h>

static void sigio_func(int);

void
sockopts(int sockfd, int doall)
{
	int 		option;
	unsigned	optlen;
	struct linger	ling;
	struct timeval	timer;
	int 		l3_prot;
	int 		rc;

	if (AF_INET == af_46) {
		l3_prot = IPPROTO_IP;
	} else {
		l3_prot = IPPROTO_IPV6;
	}
	
	/*
	 * "doall" is 0 for a server's listening socket (i.e., before
	 * accept() has returned.)  Some socket options such as SO_KEEPALIVE
	 * don't make sense at this point, while others like SO_DEBUG do.
	 */
	
	if (debug) {
		option = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_DEBUG,
			       &option, sizeof(option)) < 0)
			err_sys("SO_DEBUG setsockopt error");
		
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, SOL_SOCKET, SO_DEBUG,
			       &option, &optlen) < 0)
			err_sys("SO_DEBUG getsockopt error");
		if (option == 0)
			err_quit("SO_DEBUG not set (%d)", option);
		
		if (verbose)
			fprintf(stderr, "SO_DEBUG set\n");
	}
	
	if (dontroute) {
		option = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE,
			       &option, sizeof(option)) < 0)
			err_sys("SO_DONTROUTE setsockopt error");
		
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE,
			       &option, &optlen) < 0)
			err_sys("SO_DONTROUTE getsockopt error");
		if (option == 0)
			err_quit("SO_DONTROUTE not set (%d)", option);
		
		if (verbose)
			fprintf(stderr, "SO_DONTROUTE set\n");
	}
	
#ifdef	IP_TOS
	if ((-1 != iptos) && (0 == doall)) {
		if (IPPROTO_IP == l3_prot) {
			if (setsockopt(sockfd, l3_prot, IP_TOS,
				    &iptos, sizeof(iptos)) < 0) {
				err_sys("IP_TOS setsockopt error");
			}
			option = 0;
			optlen = sizeof(option);
			if (getsockopt(sockfd, l3_prot, IP_TOS,
				    &option, &optlen) < 0) {
				err_sys("IP_TOS getsockopt error");
			}
			if (option != iptos) {
				err_quit("IP_TOS not set (0x%02x, 0x%02x)",
				    iptos, option);
			}
			if (verbose) {
				fprintf(stderr, "IP_TOS set to 0x%02x\n",
				    iptos);
			}
		} else {
			if (setsockopt(sockfd, l3_prot, IPV6_TCLASS,
				    (char *)&iptos, sizeof(iptos)) < 0) {
				err_sys("IPv6 IPV6_TCLASS setsockopt error");
			}
			option = 0;
			optlen = sizeof(option);
			if (getsockopt(sockfd, l3_prot, IPV6_TCLASS,
				    &option, &optlen) < 0) {
				err_sys("IPV6_TCLASS getsockopt error");
			}
			if (option != iptos) {
				err_quit("IPV6_TCLASS not set (0x%02x, 0x%02x)",
				    iptos, option);
			}
			if (verbose) {
				fprintf(stderr, "IPV6_TCLASS set to 0x%02x\n",
				    iptos);
			}
		}
	}
#endif
	
#ifdef	IP_TTL
	if ((-1 != ipttl) && (0 == doall)) {
		if (IPPROTO_IP == l3_prot) {
			/* Set IPv4 TTL */
			if (setsockopt(sockfd, l3_prot, IP_TTL,
				    &ipttl, sizeof(ipttl)) < 0) {
				err_sys("IPv4 IP_TTL setsockopt error");
			}
			option = 0;
			optlen = sizeof(option);
			if (getsockopt(sockfd, l3_prot, IP_TTL,
				    &option, &optlen) < 0) {
				err_sys("IPv4 IP_TTL getsockopt error");
			}
			if (option != ipttl) {
				err_quit("IPv4 IP_TTL not set (%d)", option);
			}
			if (verbose) {
				fprintf(stderr, "IPv4 IP_TTL set to %d\n",
				    ipttl);
			}
		} else {
			/* Set IPv6 hop limit */
			if (setsockopt(sockfd, l3_prot, IPV6_UNICAST_HOPS,
				    &ipttl, sizeof(ipttl)) < 0) {
				err_sys(
				    "IPv6 IPV6_UNICAST_HOPS setsockopt error");
			}
			option = 0;
			optlen = sizeof(option);
			if (getsockopt(sockfd, l3_prot, IPV6_UNICAST_HOPS,
				    &option, &optlen) < 0) {
				err_sys(
				    "IPv6 IPV6_UNICAST_HOPS getsockopt error");
			}
			if (option != ipttl) {
				err_quit("IPv6 IPV6_UNICAST_HOPS not set (%d)",
				    option);
			}
			if (verbose) {
				fprintf(stderr,
				    "IPv6 IPV6_UNICAST_HOPS set to %d\n",
				    ipttl);
			}
		}
	}
#endif

	if ((-1 != ip_dontfrag) && (0 == doall)) {
		if (IPPROTO_IP == l3_prot) {
			/* Set the IPv4 DF (don't fragment) bit */
			if (setsockopt(sockfd, l3_prot, IP_DONTFRAG,
				    &ip_dontfrag, sizeof(ip_dontfrag)) < 0) {
				err_sys("IPv4 IP_DONTFRAG setsockopt error");
			}
			option = 0;
			optlen = sizeof(option);
			if (getsockopt(sockfd, l3_prot, IP_DONTFRAG,
				    &option, &optlen) < 0) {
				err_sys("IPv4 IP_DONTFRAG getsockopt error");
			}
			if (option != ip_dontfrag) {
				err_quit("IPv4 IP_DONTFRAG not set (%d)",
				    option);
			}
			if (verbose) {
				fprintf(stderr, "IPv4 IP_DONTFRAG set to %d\n",
				    ip_dontfrag);
			}
		} else {
			/*
			 * Disable IPv6 fragmentation
			 * Prevents the socket from fragmenting an oversized
			 * packet; write() returns an error instead.
			 */
			if (setsockopt(sockfd, l3_prot, IPV6_DONTFRAG,
				    &ip_dontfrag, sizeof(ip_dontfrag)) < 0) {
				err_sys("IPv6 IPV6_DONTFRAG setsockopt error");
			}
			option = 0;
			optlen = sizeof(option);
			if (getsockopt(sockfd, l3_prot, IPV6_DONTFRAG,
				    &option, &optlen) < 0) {
				err_sys("IPv6 IPV6_DONTFRAG getsockopt error");
			}
			if (option != ip_dontfrag) {
				err_quit("IPv6 IPV6_DONTFRAG not set (%d)",
				    option);
			}
			if (verbose) {
				fprintf(stderr,
				    "IPv6 IPV6_DONTFRAG set to %d\n",
				    ip_dontfrag);
			}
		}
	}

	if ((-1 != flowlabel_option) && (AF_INET6 == af_46)) {
		/*
		 * Enable or disable IPv6 header flow label
		 * Note:  Flow label is enabled by default in FreeBSD 8.4/10.2
		 */
		if (setsockopt(sockfd, l3_prot, IPV6_AUTOFLOWLABEL,
		    (char *)&flowlabel_option, sizeof(flowlabel_option)) < 0) {
			err_sys("IPv6 IPV6_AUTOFLOWLABEL setsockopt error");
		}
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, l3_prot, IPV6_AUTOFLOWLABEL,
		    &option, &optlen) < 0) {
			err_sys("IPv6 IPV6_AUTOFLOWLABEL getsockopt error");
		}
		if (option != flowlabel_option) {
			err_quit("IPv6 IPV6_AUTOFLOWLABEL not set (%d)",
			    option);
		}
		if (verbose) {
			fprintf(stderr,
			    "IPv6 IPV6_AUTOFLOWLABEL set to %d\n",
			    option);
		}
	}
    
	if (maxseg && l4_prot == L4_PROT_TCP) {
		/*
		 * Need to set MSS for server before connection established
		 * Beware: some kernels do not let the process set this socket
		 * option; others only let it be decreased.
		 */
		if (setsockopt(sockfd, IPPROTO_TCP, TCP_MAXSEG,
			       &maxseg, sizeof(maxseg)) < 0)
			err_sys("TCP_MAXSEG setsockopt error");
		
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, IPPROTO_TCP, TCP_MAXSEG,
			       &option, &optlen) < 0)
		    err_sys("TCP_MAXSEG getsockopt error");
		
		if (verbose)
			fprintf(stderr, "TCP_MAXSEG = %d\n", option);
	}
	
	if (sroute_cnt > 0)
		sroute_set(sockfd);
	
	if (broadcast) {
		option = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
			       &option, sizeof(option)) < 0)
			err_sys("SO_BROADCAST setsockopt error");
		
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
			       &option, &optlen) < 0)
			err_sys("SO_BROADCAST getsockopt error");
		if (option == 0)
			err_quit("SO_BROADCAST not set (%d)", option);
		
		if (verbose)
			fprintf(stderr, "SO_BROADCAST set\n");
		
#ifdef	IP_ONESBCAST
		if (onesbcast) {
			option = 1;
			if (setsockopt(sockfd, l3_prot, IP_ONESBCAST,
				       &option, sizeof(option)) < 0)
				err_sys("IP_ONESBCAST setsockopt error");
			
			option = 0;
			optlen = sizeof(option);
			if (getsockopt(sockfd, l3_prot, IP_ONESBCAST,
				       &option, &optlen) < 0)
				err_sys("IP_ONESBCAST getsockopt error");
			if (option == 0)
				err_quit("IP_ONESBCAST not set (%d)", option);
			
			if (verbose)
				fprintf(stderr, "IP_ONESBCAST set\n");
		}
#endif
	}
	
#ifdef	IP_ADD_MEMBERSHIP
	// Fixme:  Should doall be 0 or 1?
	if (joinip[0] && (doall == 0)) {
		if (IPPROTO_IP == l3_prot) {
			struct ip_mreq	mreq4;
		
			if (inet_pton(AF_INET, joinip,
				    &mreq4.imr_multiaddr) != 1) {
				err_quit("IPv4:  invalid multicast address: %s",
				    joinip);
			}
			mreq4.imr_interface.s_addr = htonl(INADDR_ANY);
			if (setsockopt(sockfd, l3_prot, IP_ADD_MEMBERSHIP,
				    &mreq4, sizeof(mreq4)) < 0) {
				err_sys(
				    "IPv4 IP_ADD_MEMBERSHIP setsockopt error");
			}
			if (verbose) {
				fprintf(stderr, "IPv4 IP_ADD_MEMBERSHIP set\n");
			}
		} else {
			struct ipv6_mreq mreq6;

			if (inet_pton(AF_INET6, joinip,
				    &mreq6.ipv6mr_multiaddr) == 0) {
				err_quit("IPv6:  invalid multicast address: %s",
				    joinip);
			}
			mreq6.ipv6mr_interface = 0;
			if (setsockopt(sockfd, l3_prot, IPV6_JOIN_GROUP,
				    &mreq6, sizeof(mreq6)) < 0) {
				err_sys(
				    "IPv6 IPV6_JOIN_GROUP setsockopt error");
			}
			if (verbose) {
				fprintf(stderr, "IPv6 IPV6_JOIN_GROUP set\n");
			}
		}
	}
#endif
	
#ifdef	IP_MULTICAST_TTL
	if (mcastttl) {
		if (IPPROTO_IP == l3_prot) {
			u_char	ttl = mcastttl;
		
			if (setsockopt(sockfd, l3_prot, IP_MULTICAST_TTL,
				       &ttl, sizeof(ttl)) < 0) {
				err_sys("IP_MULTICAST_TTL setsockopt error");
			}
			optlen = sizeof(ttl);
			if (getsockopt(sockfd, l3_prot, IP_MULTICAST_TTL,
				       &ttl, &optlen) < 0) {
				err_sys("IP_MULTICAST_TTL getsockopt error");
			}
			if (ttl != mcastttl) {
				err_quit("IP_MULTICAST_TTL not set (%d)", ttl);
			}
			if (verbose) {
				fprintf(stderr,
				    "IP_MULTICAST_TTL set to %d\n", ttl);
			}
		} else {
			// Fixme:  Should this be a char, as above?
			int	ttl = mcastttl;

			if (setsockopt(sockfd, l3_prot, IPV6_MULTICAST_HOPS,
				       (char *)&ttl, sizeof(ttl)) < 0) {
				err_sys("IPV6_MULTICAST_HOPS setsockopt error");
			}
			optlen = sizeof(ttl);
			if (getsockopt(sockfd, l3_prot, IPV6_MULTICAST_HOPS,
				       (char *)&ttl, &optlen) < 0) {
				err_sys("IPV6_MULTICAST_HOPS getsockopt error");
			}
			if (ttl != mcastttl) {
				err_quit("IPV6_MULTICAST_HOPS not set (%d)",
				    ttl);
			}
			if (verbose) {
				fprintf(stderr,
				    "IPV6_MULTICAST_HOPS set to %d\n", ttl);
			}
		}
	}
#endif
	
	if (keepalive && doall && l4_prot == L4_PROT_TCP) {
		option = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
			       &option, sizeof(option)) < 0)
			err_sys("SO_KEEPALIVE setsockopt error");
		
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
			       &option, &optlen) < 0)
			err_sys("SO_KEEPALIVE getsockopt error");
		if (option == 0)
			err_quit("SO_KEEPALIVE not set (%d)", option);
		
		if (verbose)
			fprintf(stderr, "SO_KEEPALIVE set\n");
	}
	
	if (nodelay && doall && l4_prot == L4_PROT_TCP) {
		option = 1;
		if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
			       &option, sizeof(option)) < 0)
			err_sys("TCP_NODELAY setsockopt error");
		
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
			       &option, &optlen) < 0)
			err_sys("TCP_NODELAY getsockopt error");
		if (option == 0)
			err_quit("TCP_NODELAY not set (%d)", option);
		
		if (verbose)
			fprintf(stderr, "TCP_NODELAY set\n");
	}
	
	/* just print MSS if verbose */
	if (doall && verbose && l4_prot == L4_PROT_TCP) {
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, IPPROTO_TCP, TCP_MAXSEG,
			       &option, &optlen) < 0)
			err_sys("TCP_MAXSEG getsockopt error");
		
		fprintf(stderr, "TCP_MAXSEG = %d\n", option);
	}
	
	if (linger >= 0 && doall && l4_prot == L4_PROT_TCP) {
		ling.l_onoff = 1;
		ling.l_linger = linger;		/* 0 for abortive disconnect */
		if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER,
			       &ling, sizeof(ling)) < 0)
			err_sys("SO_LINGER setsockopt error");
		
		ling.l_onoff = 0;
		ling.l_linger = -1;
		optlen = sizeof(struct linger);
		if (getsockopt(sockfd, SOL_SOCKET, SO_LINGER,
			       &ling, &optlen) < 0)
			err_sys("SO_LINGER getsockopt error");
		if (ling.l_onoff == 0 || ling.l_linger != linger)
			err_quit("SO_LINGER not set (%d, %d)", ling.l_onoff, ling.l_linger);
		
		if (verbose)
			fprintf(stderr, "linger %s, time = %d\n",
				ling.l_onoff ? "on" : "off", ling.l_linger);
	}

	if (doall && rcvtimeo) {
#ifdef	SO_RCVTIMEO
	    /* User specifies millisec, must convert to sec/usec */
		timer.tv_sec  =  rcvtimeo / 1000;
		timer.tv_usec = (rcvtimeo % 1000) * 1000;
		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
			       &timer, sizeof(timer)) < 0)
			err_sys("SO_RCVTIMEO setsockopt error");
		
		timer.tv_sec = timer.tv_usec = 0;
		optlen = sizeof(timer);
		if (getsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
			       &timer, &optlen) < 0)
			err_sys("SO_RCVTIMEO getsockopt error");
		
		if (verbose)
			fprintf(stderr, "SO_RCVTIMEO: %ld.%06ld\n",
			    (long)timer.tv_sec, timer.tv_usec);
#else
		fprintf(stderr, "warning: SO_RCVTIMEO not supported by host\n");
#endif
	}
	
	if (doall && sndtimeo) {
#ifdef	SO_SNDTIMEO
		/* User specifies millisec, must convert to sec/usec */
		timer.tv_sec  =  sndtimeo / 1000;
		timer.tv_usec = (sndtimeo % 1000) * 1000;
		if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO,
			       &timer, sizeof(timer)) < 0)
			err_sys("SO_SNDTIMEO setsockopt error");
		
		timer.tv_sec = timer.tv_usec = 0;
		optlen = sizeof(timer);
		if (getsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO,
			       &timer, &optlen) < 0)
			err_sys("SO_SNDTIMEO getsockopt error");
		
		if (verbose)
			fprintf(stderr, "SO_SNDTIMEO: %ld.%06ld\n",
			    (long)timer.tv_sec, timer.tv_usec);
#else
		fprintf(stderr, "warning: SO_SNDTIMEO not supported by host\n");
#endif
	}
	
	// Fixme:  What about SCTP?
	if (recvdstaddr && l4_prot == L4_PROT_UDP) {
#ifdef	IP_RECVDSTADDR
		option = 1;
		if (setsockopt(sockfd, l3_prot, IP_RECVDSTADDR,
			       &option, sizeof(option)) < 0)
			err_sys("IP_RECVDSTADDR setsockopt error");
		
		option = 0;
		optlen = sizeof(option);
		if (getsockopt(sockfd, l3_prot, IP_RECVDSTADDR,
			       &option, &optlen) < 0)
			err_sys("IP_RECVDSTADDR getsockopt error");
		if (option == 0)
			err_quit("IP_RECVDSTADDR not set (%d)", option);
		
		if (verbose)
			fprintf(stderr, "IP_RECVDSTADDR set\n");
#else
		fprintf(stderr, "warning: IP_RECVDSTADDR not supported by host\n");
#endif
	}
	
	if (sigio) {
#ifdef	FIOASYNC
		/*
		 * Should be able to set this with fcntl(O_ASYNC) or fcntl(FASYNC),
		 * but some systems (AIX?) only do it with ioctl().
		 *
		 * Need to set this for listening socket and for connected socket.
		 */
		signal(SIGIO, sigio_func);
		
		if (fcntl(sockfd, F_SETOWN, getpid()) < 0)
			err_sys("fcntl F_SETOWN error");
		
		option = 1;
		if (ioctl(sockfd, FIOASYNC, (char *) &option) < 0)
			err_sys("ioctl FIOASYNC error");
		
		if (verbose)
			fprintf(stderr, "FIOASYNC set\n");
#else
		fprintf(stderr, "warning: FIOASYNC not supported by host\n");
#endif
	}

	/*
	 * IPv6 hop-by-hop options extension header
	 */
	if ((doall == 1) && (ipv6_num_hopopts != -1)) {
		rc = ipv6_set_hopopts_ext_hdr(sockfd, ipv6_num_hopopts);
		if (rc != 0) {
			fprintf(stderr, "IPv6 HOPOPTS error %d\n", rc);
		} else if (verbose) {
			fprintf(stderr, "IPv6 HOPOPTS set\n");
		}
	}

	/*
	 * IPv6 destination options extension header
	 */
	if ((doall == 1) && (ipv6_num_dstopts != -1)) {
		rc = ipv6_set_dstopts_ext_hdr(sockfd, ipv6_num_dstopts);
		if (rc != 0) {
			fprintf(stderr, "IPv6 DSTOPTS error %d\n", rc);
		} else if (verbose) {
			fprintf(stderr, "IPv6 DSTOPTS set\n");
		}
	}
}

static void
sigio_func(int signo)
{
	fprintf(stderr, "SIGIO\n");
	/* shouldn't printf from a signal handler ... */
}
