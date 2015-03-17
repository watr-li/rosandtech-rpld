/*
 * prefix.c
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

#include "assert.h"
#include "prefix.h"

/* Maskbit. */
static u_char maskbit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0,
                                 0xf8, 0xfc, 0xfe, 0xff};

/* Number of bits in prefix type. */
#ifndef PNBBY
#define PNBBY 8
#endif /* PNBBY */

#define MASKBIT(offset)  ((0xff << (PNBBY - (offset))) & 0xff)

/* If n includes p prefix then return 1 else return 0. */
int
prefix_match(struct prefix *n, struct prefix *p)
{
  int offset;
  int shift;

  /* Set both prefix's head pointer. */
  u_char *np = (u_char *)&n->u.prefix;
  u_char *pp = (u_char *)&p->u.prefix;

  /* If n's prefix is longer than p's one return 0. */
  if (n->prefixlen > p->prefixlen)
    return 0;

  offset = n->prefixlen / PNBBY;
  shift =  n->prefixlen % PNBBY;

  if (shift)
    if (maskbit[shift] & (np[offset] ^ pp[offset]))
      return 0;

  while (offset--)
    if (np[offset] != pp[offset])
      return 0;
  return 1;
}

/* Copy prefix from src to dest. */
void
prefix_copy(struct prefix *dest, struct prefix *src)
{
  dest->family = src->family;
  dest->prefixlen = src->prefixlen;

 if (src->family == AF_INET6) {
   dest->u.prefix6 = src->u.prefix6;
 }
 else if (src->family == AF_UNSPEC) {
   dest->u.lp.id = src->u.lp.id;
   dest->u.lp.adv_router = src->u.lp.adv_router;
 }
 else  {
   fprintf(stderr, "prefix_copy(): Unknown address family %d",
       src->family);
   assert(0);
 }
}

/* If both prefix structure is same then return 1 else return 0. */
int
prefix_same(struct prefix *p1, struct prefix *p2)
{
  if (p1->family == p2->family && p1->prefixlen == p2->prefixlen) {
    if (p1->family == AF_INET6 )
      if (IPV6_ADDR_SAME(&p1->u.prefix, &p2->u.prefix))
        return 1;
  }
  return 0;
}

/* When both prefix structure is not same, but will be same after
   applying mask, return 0. otherwise, return 1 */
int
prefix_cmp (struct prefix *p1, struct prefix *p2)
{
  int offset;
  int shift;

  /* Set both prefix's head pointer. */
  u_char *pp1 = (u_char *)&p1->u.prefix;
  u_char *pp2 = (u_char *)&p2->u.prefix;

  if (p1->family != p2->family || p1->prefixlen != p2->prefixlen)
    return 1;

  offset = p1->prefixlen / 8;
  shift = p1->prefixlen % 8;

  if (shift)
    if (maskbit[shift] & (pp1[offset] ^ pp2[offset]))
      return 1;

  while (offset--)
    if (pp1[offset] != pp2[offset])
      return 1;

  return 0;
}

/* Allocate a new ip version 6 route */
struct prefix_ipv6 *
prefix_ipv6_new ()
{
  struct prefix_ipv6 *p;

  p = (struct prefix_ipv6 *) malloc(sizeof (struct prefix_ipv6));
  p->family = AF_INET6;
  return p;
}

/* Free prefix for IPv6. */
void
prefix_ipv6_free (struct prefix_ipv6 *p)
{
  if (p->family == AF_INET6) {
    free(p);
  }
}

/* Convert struct in6_addr netmask into integer. */
int
ip6_masklen (struct in6_addr netmask)
{
  int len = 0;
  unsigned char val;
  unsigned char *pnt;

  pnt = (unsigned char *) & netmask;

  while ((*pnt == 0xff) && len < 128)
    {
      len += 8;
      pnt++;
    }

  if (len < 128)
    {
      val = *pnt;
      while (val)
        {
          len++;
          val <<= 1;
        }
    }
  return len;
}

void
masklen2ip6 (int masklen, struct in6_addr *netmask)
{
  unsigned char *pnt;
  int bit;
  int offset;

  memset (netmask, 0, sizeof (struct in6_addr));
  pnt = (unsigned char *) netmask;

  offset = masklen / 8;
  bit = masklen % 8;

  while (offset--)
    *pnt++ = 0xff;

  if (bit)
    *pnt = maskbit[bit];
}

void
apply_mask_ipv6 (struct prefix_ipv6 *p)
{
  u_char *pnt;
  int index;
  int offset;

  index = p->prefixlen / 8;

  if (index < 16)
    {
      pnt = (u_char *) &p->prefix;
      offset = p->prefixlen % 8;

      pnt[index] &= maskbit[offset];
      index++;

      while (index < 16)
        pnt[index++] = 0;
    }
}

void
str2in6_addr (char *str, struct in6_addr *addr)
{
  int i;
  unsigned int x;

  /* %x must point to unsinged int */
  for (i = 0; i < 16; i++)
    {
      sscanf (str + (i * 2), "%02x", &x);
      addr->s6_addr[i] = x & 0xff;
    }
}

void
apply_mask (struct prefix *p)
{
  if (p->family == AF_INET6) {
    apply_mask_ipv6 ((struct prefix_ipv6 *)p);
  }
}

/* Utility function of convert between struct prefix <=> struct sockaddr_in6 */
struct prefix *
sockaddr2prefix (struct sockaddr_in6 *dest, struct sockaddr_in6 *mask)
{
  if (dest->sin6_family == AF_INET6) {
    struct prefix_ipv6 *p;

    p = prefix_ipv6_new();
    p->prefixlen = ip6_masklen(mask->sin6_addr);
    memcpy(&p->prefix, &dest->sin6_addr, sizeof(struct in6_addr));
    return (struct prefix *) p;
  }
  return NULL;
}

/* Utility function of convert between struct prefix <=> struct sockaddr_in6 */
struct prefix *
sockaddr2hostprefix (struct sockaddr_in6 *su)
{
  if (su->sin6_family == AF_INET6) {
    struct prefix_ipv6 *p;

    p = prefix_ipv6_new();
    p->family = AF_INET6;
    p->prefixlen = IPV6_MAX_BITLEN;
    memcpy(&p->prefix, &su->sin6_addr, sizeof (struct in6_addr));
    return (struct prefix *) p;
  }
  return NULL;
}

