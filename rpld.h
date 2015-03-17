/*
 * rpl-br.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * Authors: Zafi Ramarosandratana (Rosand Technologies)
 *
 */

#ifndef _RPL_BR_H
#define _RPL_BR_H

#include "netdrv.h"

PROCESS_NAME(border_router_process);


/* RPLD message types. */
#define RPLD_INTERFACE_ADD                1
#define RPLD_INTERFACE_DELETE             2
#define RPLD_INTERFACE_ADDRESS_ADD        3
#define RPLD_INTERFACE_ADDRESS_DELETE     4
#define RPLD_INTERFACE_UP                 5
#define RPLD_INTERFACE_DOWN               6
#define RPLD_IPV4_ROUTE_ADD               7
#define RPLD_IPV4_ROUTE_DELETE            8
#define RPLD_IPV6_ROUTE_ADD               9
#define RPLD_IPV6_ROUTE_DELETE           10
#define RPLD_REDISTRIBUTE_ADD            11
#define RPLD_REDISTRIBUTE_DELETE         12
#define RPLD_REDISTRIBUTE_DEFAULT_ADD    13
#define RPLD_REDISTRIBUTE_DEFAULT_DELETE 14
#define RPLD_IPV4_NEXTHOP_LOOKUP         15
#define RPLD_IPV6_NEXTHOP_LOOKUP         16
#define RPLD_IPV4_IMPORT_LOOKUP          17
#define RPLD_IPV6_IMPORT_LOOKUP          18
#define RPLD_MESSAGE_MAX                 19

/* RPLD route's types. */
#define RPLD_ROUTE_SYSTEM               0
#define RPLD_ROUTE_KERNEL               1
#define RPLD_ROUTE_CONNECT              2
#define RPLD_ROUTE_STATIC               3
#define RPLD_ROUTE_RIP                  4
#define RPLD_ROUTE_RIPNG                5
#define RPLD_ROUTE_OSPF                 6
#define RPLD_ROUTE_OSPF6                7
#define RPLD_ROUTE_BGP                  8
#define RPLD_ROUTE_MAX                  9

/* RPLD's family types. */
#define RPLD_FAMILY_IPV4                1
#define RPLD_FAMILY_IPV6                2
#define RPLD_FAMILY_MAX                 3

/* Error codes of RPLD. */
#define RPLD_ERR_RTEXIST               -1
#define RPLD_ERR_RTUNREACH             -2
#define RPLD_ERR_EPERM                 -3
#define RPLD_ERR_RTNOEXIST             -4

/* RPLD message flags */
#define RPLD_FLAG_INTERNAL           0x01
#define RPLD_FLAG_SELFROUTE          0x02
#define RPLD_FLAG_BLACKHOLE          0x04
#define RPLD_FLAG_IBGP               0x08
#define RPLD_FLAG_SELECTED           0x10
#define RPLD_FLAG_CHANGED            0x20
#define RPLD_FLAG_STATIC             0x40

/* RPLD nexthop flags. */
#define RPLD_NEXTHOP_IFINDEX            1
#define RPLD_NEXTHOP_IFNAME             2
#define RPLD_NEXTHOP_IPV4               3
#define RPLD_NEXTHOP_IPV4_IFINDEX       4
#define RPLD_NEXTHOP_IPV4_IFNAME        5
#define RPLD_NEXTHOP_IPV6               6
#define RPLD_NEXTHOP_IPV6_IFINDEX       7
#define RPLD_NEXTHOP_IPV6_IFNAME        8
#define RPLD_NEXTHOP_BLACKHOLE          9

/* Address family numbers from RFC1700. */
#define AFI_IP6                   1
#define AFI_MAX                   2

/* Subsequent Address Family Identifier. */
#define SAFI_UNICAST              1
#define SAFI_MULTICAST            2
#define SAFI_UNICAST_MULTICAST    3
#define SAFI_MPLS_VPN             4
#define SAFI_MAX                  5

/* Filter direction.  */
#define FILTER_IN                 0
#define FILTER_OUT                1
#define FILTER_MAX                2

/* Flag manipulation macros. */
#define CHECK_FLAG(V,F)      ((V) & (F))
#define SET_FLAG(V,F)        (V) = (V) | (F)
#define UNSET_FLAG(V,F)      (V) = (V) & ~(F)

typedef uint16_t afi_t;
typedef uint8_t safi_t;

#endif /* _RPL_BR_H */
