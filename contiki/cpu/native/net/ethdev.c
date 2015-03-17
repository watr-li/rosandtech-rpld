/*
 * ethdev.c
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
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if_arp.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>

#include "contiki-net.h"

#include "ethdev.h"

#define BUF ((struct ethhdr *)&uip_buf[0])
#define IPBUF ((struct ip6_hdr *)&uip_buf[ETH_HLEN])

/***********************************************************************************
 * GLOBAL VARIABLES
 */
process_event_t ethnet_event;

/***********************************************************************************
 * LOCAL DATA
 */
static struct interface *iface;
static uint8_t output(uip_lladdr_t *dst);

/*----------------------------------------------------------------------*/
static void
setoutput(void)
{
  process_start(&ethdev_process, NULL);

  tcpip_set_outputfunc(output);
}

/*----------------------------------------------------------------------*/
static int
setup(struct interface *ni)
{
  int nd_socket;
  int verbose = ni->verbose;

  if (verbose) {
    fprintf(stderr, "setting up %s\n", ni->name);
  }

  nd_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

  if (nd_socket < 0) {
    fprintf(stderr, "can't create socket(AF_PACKET): %s\n", strerror(errno));
    return -1;
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
  struct sockaddr_ll saddr;
  int ret;

  /*
   * If L3 dest is multicast, build L2 multicast address
   * as per RFC 2464 section 7
   * else fill with the address in argument
   */
  if (dst == NULL) {
    if (iface->verbose > 3) {
      fprintf(stderr, "(%s) - build L2 dest multicast address\n", iface->name);
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

  /* Prepare sockaddr_ll */
  memset(&saddr, 0, sizeof(saddr));                     /* Clear to zero sockaddr_ll */
  saddr.sll_family = PF_PACKET;                         /* Raw communication */
  saddr.sll_protocol = htons(ETH_P_IPV6);               /* IPv6 Protocol */
  saddr.sll_ifindex = iface->ifindex;                   /* Index of the network device */
  saddr.sll_hatype = ARPHRD_ETHER;                      /* ARP hardware identifier is ethernet */
  saddr.sll_pkttype =  PACKET_OUTGOING;                 /* Outgoing of any type */
  saddr.sll_halen = ETH_ALEN;                           /* Address length */
  memcpy(saddr.sll_addr, BUF->h_dest, ETH_ALEN);        /* Destination MAC address */

  ret = sendto(iface->nd_socket, uip_buf, uip_len, 0,
      (struct sockaddr *) &saddr, sizeof(saddr));
  if (ret == -1) {
    perror("send packet");
    exit(errno);
  }
  return ret;
}

/*---------------------------------------------------------------------------*/
static void
pollhandler(void)
{
  int len;
  struct sockaddr_ll saddr;

  unsigned char buf[iface->if_mtu];

  process_poll(&ethdev_process);

  if (poll() > 0) {
    int saddr_len;
    saddr_len = sizeof(struct sockaddr_ll);
    len = recvfrom(iface->nd_socket, buf, iface->if_mtu, 0,
        (struct sockaddr *) &saddr,  (socklen_t *) &saddr_len);

    if (saddr.sll_ifindex != iface->ifindex) {
      return;
    }

    if ((saddr.sll_pkttype != PACKET_HOST) &&
        (saddr.sll_pkttype != PACKET_MULTICAST)) {
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
ethnet_exit(char *st)
{
  fprintf(stderr, "%s process exited\n", st);
}

/*---------------------------------------------------------------------------*/
PROCESS(ethdev_process, "Ethernet device process");

PROCESS_THREAD(ethdev_process, ev, data)
{
  PROCESS_POLLHANDLER(pollhandler());
  PROCESS_EXITHANDLER(ethnet_exit("ETHDEV"))

  PROCESS_BEGIN();
  process_poll(&ethdev_process);

  PROCESS_WAIT_UNTIL(ev == PROCESS_EVENT_EXIT);

  PROCESS_END();
}

struct netdrv ethdrv = {
    "ethernet",
    setup,
    setoutput
};
