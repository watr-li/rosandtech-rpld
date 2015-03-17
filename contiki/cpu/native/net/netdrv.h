/*
 * netdrv.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * Authors: Zafi Ramarosandratana (Rosand Technologies)
 *
 */


#ifndef _NETDEV_H
#define _NETDEV_H

#include <netinet/ip6.h>
#include <linux/if.h>             /* for IFNAMSIZ */

#define RPLD_FLAGS    1

extern char  progbuf[];

struct interface {
  char               name[IFNAMSIZ];           // Interface name
  int                ifindex;                  // Interface index
  int                if_mtu;                   // Interface MTU
  struct in6_addr    if_laddr;                 // Interface IPv6 link-local address
  int                if_naddr;                 // Interface number of IPv6 addresses
  struct in6_addr    if_gaddr[5];              // Global IPv6 address
  struct in6_addr   *prefix;                   // Assigned IPv6 prefix address
  struct in6_addr   *dagid;                    // Assigned DAG ID
  unsigned long      flags;                    // Interface flags
  int                metric;                   // Interface metric

  /* Socket descriptor */
  int                nd_socket;

  /* Interface to netlink */
  unsigned char      eui48[6];                 // EU-48 MAC address

  int                verbose;

  /* Next in the list */
  struct interface  *next;
};

struct netdrv {
  char *name;
  int (* init)(struct interface *);
  void (* setup)(void);
};

int scan_net_devices(int verbose);
struct interface *if_get_by_index(int);
struct interface *if_get_by_name(char *);
struct interface *if_get_active(void);
int if_is_up(struct interface *);

#endif /* _NETDEV_H */
