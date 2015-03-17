/*
 * tundev.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * Authors: Zafi Ramarosandratana (Rosand Technologies)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#include <linux/if_ether.h>
#include <arpa/inet.h>

#include "contiki-net.h"

#include "sundev.h"

#define BUF ((struct ethhdr *)&uip_buf[0])
#define IPBUF ((struct ip6_hdr *)&uip_buf[ETH_HLEN])

/***********************************************************************************
 * GLOBAL VARIABLES
 */

/***********************************************************************************
 * LOCAL DATA
 */
static struct interface *iface;
static uint8_t output(uip_lladdr_t *dst);

/*----------------------------------------------------------------------*/
static void
setoutput(void)
{
  process_start(&sundev_process, NULL);

  if (iface->flags & RPLD_FLAGS) {
    tcpip_set_outputfunc(output);
  }
}

/*----------------------------------------------------------------------*/
static int
setup(struct interface *ni)
{
  int nd_socket;
  int verbose = ni->verbose;
  struct sockaddr_un sock_un;
  int len;
  char sockname[256];
  int backlog = 5;

  if (verbose) {
    fprintf(stderr, "setting up %s\n", ni->name);
  }

  nd_socket = socket(AF_UNIX, SOCK_STREAM, 0);

  if (nd_socket < 0) {
    fprintf(stderr, "can't create socket: %s\n", strerror(errno));
    return -1;
  }

  strcpy(sockname, SOCK_PATH);
  strcat(sockname, ".");
  strcat(sockname, ni->name);
  sock_un.sun_family = AF_UNIX;
  strcpy(sock_un.sun_path, sockname);
  len = strlen(sock_un.sun_path) + sizeof(sock_un.sun_family);
  if (ni->flags & RPLD_FLAGS) {
    int s, t;
    struct sockaddr_un remote;

    s = nd_socket;
    unlink(sock_un.sun_path);
    if (bind(s, (struct sockaddr *) &sock_un, len) < 0) {
      fprintf(stderr, "can't bind socket: %s\n", strerror(errno));
      return -1;
    }
    if (listen(s, backlog) < 0) {
      fprintf(stderr, "can't listen on socket: %s\n", strerror(errno));
      return -1;
    }
    t = sizeof(remote);
    nd_socket = accept(s, (struct sockaddr *) & remote, (socklen_t *) &t);
    if (nd_socket < 0) {
      fprintf(stderr, "can't accept on socket: %s", strerror(errno));
      return -1;
    }
  }
  else {
    if (connect(nd_socket, (struct sockaddr *) &sock_un, len) < 0) {
      fprintf(stderr, "can't connect to socket: %s\n", strerror(errno));
      return -1;
    }
  }
  ni->nd_socket = nd_socket;
  iface = ni;

  return nd_socket;
}

/*---------------------------------------------------------------------------*/
static int
poll(void)
{
  fd_set rfds;
  struct timeval tv;
  int ret;

  FD_ZERO(&rfds);
  FD_SET(iface->nd_socket, &rfds);

  /* Wait up to five milliseconds. */
  tv.tv_sec = 0;
  tv.tv_usec = 5000;

  /* Watch socket to see when it has input */
  ret = select(iface->nd_socket+1, &rfds, NULL, NULL, &tv);
  if (ret == -1) {
    perror("receive packet");
    exit(errno);
  }
  return ret;
}

/*----------------------------------------------------------------------*/
static uint8_t
output(uip_lladdr_t *dst)
{
  struct in6_addr *iaddr = &IPBUF->ip6_dst;
  int ret;

  /*
   * If L3 dest is multicast, build L2 multicast address
   * as per RFC 2464 section 7
   * else fill with the address in argument
   */
  if (dst == NULL) {
    if (iface->verbose > 3) {
      fprintf(stderr, "(%s)- build L2 dest multicast address\n", iface->name);
    }
    BUF->h_dest[0] = 0x33;
    BUF->h_dest[1] = 0x33;
    BUF->h_dest[2] = iaddr->s6_addr[12];
    BUF->h_dest[3] = iaddr->s6_addr[13];
    BUF->h_dest[4] = iaddr->s6_addr[14];
    BUF->h_dest[5] = iaddr->s6_addr[15];
  } else {
    memcpy(BUF->h_dest, dst, ETH_ALEN);
  }
  memcpy(BUF->h_source, iface->eui48, ETH_ALEN);
  BUF->h_proto = htons(ETH_P_IPV6);

  uip_len += sizeof(struct ethhdr);                     /* Add Ethernet packet header */
  if (iface->verbose > 2) {
    int i;
    char src_addrbuf[INET6_ADDRSTRLEN], dst_addrbuf[INET6_ADDRSTRLEN];

    inet_ntop(AF_INET6, &IPBUF->ip6_src, src_addrbuf, INET6_ADDRSTRLEN);
    inet_ntop(AF_INET6, &IPBUF->ip6_dst, dst_addrbuf, INET6_ADDRSTRLEN);

    fprintf(stderr, "(%s) - sending packet (len: %d) from %s to %s\n",
        iface->name, uip_len, src_addrbuf, dst_addrbuf);
    if (iface->verbose > 3) {
      for (i=0; i<uip_len; i++) {
        fprintf(stderr, "%02x", uip_buf[i]);
      }
      fprintf(stderr, "\n");
    }
  }

  ret = send(iface->nd_socket, uip_buf, uip_len, 0);
  if (ret <= 0) {
    fprintf(stderr, "(%s) - %s\n", iface->name, strerror(errno));
  }
  return ret;
}

/*---------------------------------------------------------------------------*/
static void
pollhandler(void)
{
  unsigned char buf[iface->if_mtu];

  process_poll(&sundev_process);

  if (poll() > 0) {
    int len;

    len = recv(iface->nd_socket, buf, iface->if_mtu, 0);
    if (len == 0) {
      return;
    }

    if (iface->flags & RPLD_FLAGS) {
      struct ip6_hdr *ip6;
      ip6 = (struct ip6_hdr *) &buf[sizeof(struct ethhdr)];
      if (ip6->ip6_ctlun.ip6_un1.ip6_un1_nxt != IPPROTO_ICMPV6) {
        return;
      }
    }

    memcpy(uip_buf, buf, len);
    uip_len = len - sizeof(struct ethhdr);

    if (iface->verbose > 2) {
      int i;
      char src_addrbuf[INET6_ADDRSTRLEN], dst_addrbuf[INET6_ADDRSTRLEN];

      inet_ntop(AF_INET6, &IPBUF->ip6_src, src_addrbuf, INET6_ADDRSTRLEN);
      inet_ntop(AF_INET6, &IPBUF->ip6_dst, dst_addrbuf, INET6_ADDRSTRLEN);

      fprintf(stderr, "(%s) - received packet (len: %d) from %s to %s\n",
          iface->name, len, src_addrbuf, dst_addrbuf);
      if (iface->verbose > 3) {
        for (i=0; i<len; i++) {
          fprintf(stderr, "%02x", buf[i]);
        }
        fprintf(stderr, "\n");
      }
    }
    process_post(PROCESS_BROADCAST, ethnet_event, 0);
  }
}

/*---------------------------------------------------------------------------*/
static void
sunnet_exit(char *st)
{
  fprintf(stderr, " %s process exited\n", st);
}

/*---------------------------------------------------------------------------*/
PROCESS(sundev_process, "NAMED PIPE device process");

PROCESS_THREAD(sundev_process, ev, data)
{
  PROCESS_POLLHANDLER(pollhandler());
  PROCESS_EXITHANDLER(sunnet_exit("PIPEDEV"))

  PROCESS_BEGIN();
  process_poll(&sundev_process);

  PROCESS_WAIT_UNTIL(ev == PROCESS_EVENT_EXIT);

  PROCESS_END();
}

struct netdrv sundrv = {
    "tap",
    setup,
    setoutput
};
