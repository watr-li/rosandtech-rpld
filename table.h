/*
 * table.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * Authors: Zafi Ramarosandratana (Rosand Technologies)
 *
 */

#ifndef _TABLE_H
#define _TABLE_H

#include "prefix.h"

/* Routing table top structure. */
struct route_table
{
  struct route_node *top;
};

/* Each routing entry. */
struct route_node
{
  /* Actual prefix of this radix. */
  struct prefix p;

  /* Tree link. */
  struct route_table *table;
  struct route_node *parent;
  struct route_node *link[2];
#define l_left   link[0]
#define l_right  link[1]

  /* Lock of this radix */
  unsigned int lock;

  /* Each node of route. */
  void *info;

  /* Aggregation. */
  void *aggregate;
};

/* Prototypes. */
struct route_table *route_table_init(void);
struct route_node * route_node_set(struct route_table *table, struct prefix *prefix);
void route_table_unlock(struct route_table *);
void route_table_finish (struct route_table *);
void route_node_unlock (struct route_node *node);
void route_node_delete (struct route_node *node);
struct route_node *route_top (struct route_table *);
struct route_node *route_next (struct route_node *);
struct route_node *route_find_next(struct route_node *node);
struct route_node *route_next_until (struct route_node *, struct route_node *);
struct route_node *route_node_get (struct route_table *, struct prefix *);
struct route_node *route_node_lookup (struct route_table *, struct prefix *);
struct route_node *route_node_lock (struct route_node *node);
void route_node_dump (struct route_table *t);
struct route_node *route_node_match (struct route_table *, struct prefix *);
struct route_node *route_node_match_ipv6 (struct route_table *,
                                          struct in6_addr *);

#endif /* _TABLE_H */
