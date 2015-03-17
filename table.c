/*
 * table.c
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

#include "prefix.h"
#include "table.h"
#include "memory.h"

#include "assert.h"

void route_table_free(struct route_table *);

struct route_table *
route_table_init(void)
{
  struct route_table *rt;

  rt = (struct route_table *) malloc(sizeof (struct route_table));
  memset(rt, 0, sizeof (struct route_table));
  return rt;
}

void
route_table_unlock(struct route_table *t)
{
  struct route_node *node;

  for (node = route_top(t); node != NULL; node = route_next(node))  {
    if (node == NULL) {
      break;
    }
    if (node->lock > 0)
      node->lock--;
  }
}

void
route_table_finish(struct route_table *rt)
{
  route_table_free(rt);
}

/* Allocate new route node. */
struct route_node *
route_node_new(void)
{
  struct route_node *node;

  node = (struct route_node *) malloc(sizeof (struct route_node));
  memset(node, 0, sizeof (struct route_node));
  return node;
}

/* Allocate new route node with prefix set. */
struct route_node *
route_node_set(struct route_table *table, struct prefix *prefix)
{
  struct route_node *node;

  node = route_node_new();

  prefix_copy(&node->p, prefix);
  node->table = table;

  return node;
}

/* Free route node. */
void
route_node_free (struct route_node *node)
{
  free(node);
}

/* Free route table. */
void
route_table_free(struct route_table *rt)
{
  struct route_node *tmp_node;
  struct route_node *node;

  if (rt == NULL)
    return;

  node = rt->top;

  while (node) {
    if (node->l_left) {
      node = node->l_left;
      continue;
    }

    if (node->l_right) {
      node = node->l_right;
      continue;
    }

    tmp_node = node;
    node = node->parent;

    if (node != NULL) {
      if (node->l_left == tmp_node)
        node->l_left = NULL;
      else
        node->l_right = NULL;

      route_node_free (tmp_node);
    }
    else {
      route_node_free (tmp_node);
      break;
    }
  }

  free(rt);
  return;
}

/* Utility mask array. */
static u_char maskbit[] =
{
  0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
};

/* Common prefix route generation. */
static void
route_common(struct prefix *n, struct prefix *p, struct prefix *new)
{
  int i;
  u_char diff;
  u_char mask;

  u_char *np = (u_char *) &n->u.prefix;
  u_char *pp = (u_char *) &p->u.prefix;
  u_char *newp = (u_char *) &new->u.prefix;

  for (i = 0; i < p->prefixlen / 8; i++) {
    if (np[i] == pp[i])
      newp[i] = np[i];
    else
      break;
  }

  new->prefixlen = i * 8;

  if (new->prefixlen != p->prefixlen) {
    diff = np[i] ^ pp[i];
    mask = 0x80;
    while (new->prefixlen < p->prefixlen && !(mask & diff)) {
      mask >>= 1;
      new->prefixlen++;
    }
    newp[i] = np[i] & maskbit[new->prefixlen % 8];
  }
}

/* Check bit of the prefix. */
static int
check_bit(u_char *prefix, u_char prefixlen)
{
  int offset;
  int shift;
  u_char *p = (u_char *)prefix;

  assert(prefixlen <= 128);

  offset = prefixlen / 8;
  shift = 7 - (prefixlen % 8);

  return (p[offset] >> shift & 1);
}

static void
set_link(struct route_node *node, struct route_node *new)
{
  int bit;

  bit = check_bit(&new->p.u.prefix, node->p.prefixlen);

  assert(bit == 0 || bit == 1);

  node->link[bit] = new;
  new->parent = node;
}

/* Lock node. */
struct route_node *
route_node_lock (struct route_node *node)
{
  node->lock++;
  return node;
}

/* Unlock node. */
void
route_node_unlock (struct route_node *node)
{
  if (node->lock)
      node->lock--;

  if (node->lock == 0)
    route_node_delete(node);
}

/* Dump routing table. */
void
route_node_dump (struct route_table *t)
{
  struct route_node *node;
  char buf[INET6_ADDRSTRLEN];

  for (node = route_top(t); node != NULL; node = route_next(node))  {
    fprintf (stderr, "[%d] %p %s/%d\n",
        node->lock,
        node->info,
        inet_ntop (node->p.family, &node->p.u.prefix, buf, INET6_ADDRSTRLEN),
        node->p.prefixlen);
  }
}

/* Find matched prefix. */
struct route_node *
route_node_match (struct route_table *table, struct prefix *p)
{
  struct route_node *node;
  struct route_node *matched;

  matched = NULL;
  node = table->top;

  /* Walk down tree.  If there is matched route then store it to
     matched. */
  while (node && node->p.prefixlen <= p->prefixlen &&
         prefix_match (&node->p, p)) {
    if (node->info)
      matched = node;
    node = node->link[check_bit(&p->u.prefix, node->p.prefixlen)];
  }

  /* Return matched route. */
  return matched;
}

struct route_node *
route_node_match_ipv6 (struct route_table *table, struct in6_addr *addr)
{
  struct prefix_ipv6 p;

  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = IPV6_MAX_PREFIXLEN;
  p.prefix = *addr;

  return route_node_match (table, (struct prefix *) &p);
}

/* Lookup same prefix node.  Return NULL when we can't find route. */
struct route_node *
route_node_lookup (struct route_table *table, struct prefix *p)
{
  struct route_node *node;

  node = table->top;

  while (node && node->p.prefixlen <= p->prefixlen &&
         prefix_match (&node->p, p)) {
    if (node->p.prefixlen == p->prefixlen && node->info)
      return node;

    node = node->link[check_bit(&p->u.prefix, node->p.prefixlen)];
  }

  return NULL;
}

/* Add node to routing table. */
struct route_node *
route_node_get (struct route_table *table, struct prefix *p)
{
  struct route_node *new;
  struct route_node *node;
  struct route_node *match;

  match = NULL;
  node = table->top;
  while (node && node->p.prefixlen <= p->prefixlen &&
         prefix_match (&node->p, p)) {
    if (node->p.prefixlen == p->prefixlen)
      return node;

    match = node;
    node = node->link[check_bit(&p->u.prefix, node->p.prefixlen)];
  }

  if (node == NULL) {
    new = route_node_set(table, p);
    if (match)
      set_link(match, new);
    else
      table->top = new;
  }
  else  {
    new = route_node_new();
    route_common(&node->p, p, &new->p);
    new->p.family = p->family;
    new->table = table;
    set_link(new, node);

    table->top = new;

    if (new->p.prefixlen != p->prefixlen)  {
      match = new;
      new = route_node_set(table, p);
      set_link(match, new);
    }
  }

  return new;
}

/* Delete node from the routing table. */
void
route_node_delete (struct route_node *node)
{
  struct route_node *child;
  struct route_node *parent;

  assert (node->lock == 0);
  assert (node->info == NULL);

  if (node->l_left && node->l_right)
    return;

  if (node->l_left)
    child = node->l_left;
  else
    child = node->l_right;

  parent = node->parent;

  if (child)
    child->parent = parent;

  if (parent) {
    if (parent->l_left == node)
      parent->l_left = child;
    else
      parent->l_right = child;
  }
  else
    node->table->top = child;

  route_node_free (node);

  /* If parent node is stub then delete it also. */
  if (parent && parent->lock == 0)
    route_node_delete (parent);
}

/* Get first node.  This function is useful when one want
   to lookup all the node exist in the routing table. */
struct route_node *
route_top (struct route_table *table)
{
  /* Return the top node. */
  return table->top;
}

/* Return next node. */
struct route_node *
route_next (struct route_node *node)
{
  if (node->l_left)  {
    return node->l_left;
  }
  if (node->l_right)  {
    return node->l_right;
  }

  while (node->parent)  {
    if (node->parent->l_left == node && node->parent->l_right)  {
      return node->parent->l_right;
    }
    node = node->parent;
  }

  return NULL;
}

/* Look for next node until limit. */
struct route_node *
route_next_until (struct route_node *node, struct route_node *limit)
{
  if (node->l_left)  {
    return node->l_left;
  }
  if (node->l_right)  {
    return node->l_right;
  }

  while (node->parent && node != limit)  {
    if (node->parent->l_left == node && node->parent->l_right)  {
      return node->parent->l_right;
    }
    node = node->parent;
  }
  return NULL;
}
