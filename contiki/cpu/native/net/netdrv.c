/*
 * netdev.c
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
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>

#include <libnetlink.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <arpa/inet.h>

#include "netdrv.h"

static struct interface nli;

/*----------------------------------------------------------------------*/
struct interface *
if_get_by_index(int index)
{
  struct interface *ni = &nli;

  do {
    if(ni->ifindex == index) {
      break;
    }
    ni = ni->next;
  } while (ni != NULL);

  return ni;
}

/*----------------------------------------------------------------------*/
struct interface *
if_get_by_name(char *name)
{
  struct interface *ni = &nli;

  do {
    if (strcmp(name, ni->name) ==  0) {
      break;
    }
    ni = ni->next;
  } while (ni != NULL);

  return ni;
}

/*----------------------------------------------------------------------*/
struct interface *
if_get_active(void)
{
  struct interface *ni = &nli;

  do {
    if (ni->nd_socket != -1) {
      break;
    }
    ni = ni->next;
  } while (ni != NULL);
  return ni;
}

/*----------------------------------------------------------------------*/
int
if_is_up(struct interface *ifp)
{
  return ifp->flags & IFF_UP;
}
/*----------------------------------------------------------------------*/
static int
add_linkinfo(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
  int *verbose = (int *) arg;

  struct ifinfomsg *ifi = (struct ifinfomsg *) NLMSG_DATA(n);
  struct rtattr *tb[IFLA_MAX+1];
  int len = n->nlmsg_len;
  unsigned char *addr = NULL;
  unsigned int addrlen = 0;

  struct interface *ni;

  len -= NLMSG_LENGTH(sizeof(*ifi));
  if (len < 0) {
    return -1;
  }

  parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
  if (tb[IFLA_IFNAME] == NULL) {
      fprintf(stderr, "BUG: nil ifname\n");
      return -1;
  }

  ni = if_get_by_index(ifi->ifi_index);
  if (ni == NULL){
    ni = &nli;
    while (ni->next != NULL) {
      ni = ni->next;
    }
    ni->next = (struct interface *) malloc(sizeof(struct interface));
    if (ni->next == NULL) {
      fprintf(stderr, "Create network interface list failed.\n");
      return -1;
    }
    memset(ni->next, 0, sizeof(struct interface));

    ni->ifindex = ifi->ifi_index;
  }
  strcpy(ni->name, RTA_DATA(tb[IFLA_IFNAME]));

  addr = (unsigned char *)RTA_DATA(tb[IFLA_ADDRESS]);
  if(addr) {
    addrlen = RTA_PAYLOAD(tb[IFLA_ADDRESS]);
    memcpy(ni->eui48, addr, addrlen);
  }

  if (tb[IFLA_MTU]) {
      ni->if_mtu =  *(int *)RTA_DATA(tb[IFLA_MTU]);
  }

  /* socket is not created */
  ni->nd_socket = -1;
  ni->verbose = *verbose;
  ni->if_naddr = 0;
  ni->metric = 0;
  ni->prefix = NULL;

  if (ni->verbose > 3) {
    int i;

    fprintf(stderr, "network device ");
    fprintf(stderr, "index = %d, name = %s", ni->ifindex, ni->name);
    fprintf(stderr, ", maxmtu = %d", ni->if_mtu);
    fprintf(stderr, ", hw address = ");
    for (i=0; i < addrlen; i++) {
      fprintf(stderr, "%02x", ni->eui48[i]);
      if (i < addrlen-1) {
        fprintf(stderr, ":");
      }
    }
    fprintf(stderr, "\n");
  }
  return 0;
}

/*----------------------------------------------------------------------*/
static int
add_ipinfo(const struct sockaddr_nl *who, struct nlmsghdr *n)
{
  struct ifaddrmsg *iai = (struct ifaddrmsg *)NLMSG_DATA(n);
  struct rtattr *tb[IFA_MAX+1], *addrattr;
  int len = n->nlmsg_len;

  struct interface *ni;
  unsigned char *addr = NULL;
  unsigned int addrlen = 0;

  len -= NLMSG_LENGTH(sizeof(*iai));
  if (len < 0) {
    return -1;
  }

  parse_rtattr(tb, IFA_MAX, IFA_RTA(iai), len);
  addrattr = tb[IFA_LOCAL];
  if(addrattr == NULL) {
      addrattr = tb[IFA_ADDRESS];
  }

  if (addrattr == NULL) {
      /* not a useful update */
      return 0;
  }

  ni = if_get_by_index(iai->ifa_index);
  if (ni == NULL) {
    return 0;
  }

  addr = (unsigned char *)RTA_DATA(addrattr);
  if(addr) {
    addrlen = RTA_PAYLOAD(addrattr);
    if(addrlen > sizeof(struct in6_addr)) {
      addrlen=sizeof(struct in6_addr);
    }
    if ((iai->ifa_scope & 0xFC) == 0xFC) {        /* Scope: link local IPV6 address */
      memcpy(&ni->if_laddr, addr, addrlen);
    } else {                                      /* Scope: global IPV6 address */
      if (ni->if_naddr == 5) {
        return 0;
      }
      memcpy(&ni->if_gaddr[ni->if_naddr], addr, addrlen);
      ni->if_naddr++;
    }

    if (ni->verbose > 3) {
      char b1[INET6_ADDRSTRLEN];

      fprintf(stderr, "index = %d, name = %s, scope = %s, address = %s\n",
          ni->ifindex, ni->name,
          ((iai->ifa_scope & 0xfc) == 0xfc) ? "link-local":"global",
          inet_ntop(AF_INET6, addr, b1, INET6_ADDRSTRLEN));
    }
  }

  return 0;
}

/*----------------------------------------------------------------------*/
int
scan_net_devices(int verbose)
{
  struct rtnl_handle *nl_handle;
  int fd;

  nl_handle = (struct rtnl_handle *) malloc (sizeof(struct rtnl_handle));
  if (nl_handle == NULL) {
    perror("Cannot allocate memory");
    return -1;
  }

  fd = rtnl_open(nl_handle, 0);
  if (fd < 0) {
    fprintf (stderr, "Cannot open rtnetlink.\n");
    free(nl_handle);
    return -1;
  }

  /* Get list of interfaces */
  if (rtnl_wilddump_request(nl_handle, AF_PACKET, RTM_GETLINK) < 0) {
    perror("Cannot send dump request");
    free(nl_handle);
    return -1;
  }

  if (rtnl_dump_filter(nl_handle, (rtnl_filter_t) add_linkinfo,
      (void *)&verbose, NULL, NULL) < 0) {
    fprintf(stderr, "Dump terminated\n");
    free(nl_handle);
    return -1;
  }

  /* Get IPV6 addresses of each interface */
  if (rtnl_wilddump_request(nl_handle, AF_INET6, RTM_GETADDR) < 0) {
    perror("Cannot send dump request");
    free(nl_handle);
    return -1;
  }

  if (rtnl_dump_filter(nl_handle, (rtnl_filter_t) add_ipinfo,
      NULL, NULL, NULL) < 0) {
    fprintf(stderr, "Dump terminated\n");
    free(nl_handle);
    return -1;
  }

  rtnl_close(nl_handle);
  free(nl_handle);

  return fd;
}
