/*
 * prefix.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * Authors: Zafi Ramarosandratana (Rosand Technologies)
 *
 */

#include <string.h>
#include <arpa/inet.h>

#ifndef _PREFIX_H
#define _PREFIX_H

struct prefix {
  u_char family;
  u_char prefixlen;
  union {
    u_char prefix;
    struct in6_addr prefix6;
    struct {
      struct in_addr id;
      struct in_addr adv_router;
    } lp;
    u_char val[8];
  } u __attribute__ ((aligned (8)));
};

/* IPv6 prefix structure. */
struct prefix_ipv6
{
  u_char family;
  u_char prefixlen;
  struct in6_addr prefix __attribute__ ((aligned (8)));
};

/* Max bit/byte length of IPv6 address. */
#define IPV6_MAX_BYTELEN    16
#define IPV6_MAX_BITLEN    128
#define IPV6_MAX_PREFIXLEN 128
#define IPV6_ADDR_CMP(D,S)   memcmp ((D), (S), IPV6_MAX_BYTELEN)
#define IPV6_ADDR_SAME(D,S)  (memcmp ((D), (S), IPV6_MAX_BYTELEN) == 0)
#define IPV6_ADDR_COPY(D,S)  memcpy ((D), (S), IPV6_MAX_BYTELEN)

/* Count prefix size from mask length */
#define PSIZE(a) (((a) + 7) / (8))

/* Prefix's family member. */
#define PREFIX_FAMILY(p)  ((p)->family)

int prefix_match(struct prefix *n, struct prefix *p);
void prefix_copy(struct prefix *dest, struct prefix *src);
int prefix_same(struct prefix *p1, struct prefix *p2);
int prefix_cmp (struct prefix *p1, struct prefix *p2);

struct prefix_ipv6 * prefix_ipv6_new();
void prefix_ipv6_free(struct prefix_ipv6 *p);
int ip6_masklen(struct in6_addr netmask);
void masklen2ip6(int masklen, struct in6_addr *netmask);
void apply_mask_ipv6(struct prefix_ipv6 *p);
void str2in6_addr(char *str, struct in6_addr *addr);

void apply_mask (struct prefix *p);
struct prefix * sockaddr2prefix (struct sockaddr_in6 *dest, struct sockaddr_in6 *mask);
struct prefix * sockaddr2hostprefix (struct sockaddr_in6 *su);

#endif /* _PREFIX_H */
