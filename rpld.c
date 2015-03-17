/*
 * rpl-br.c
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
#include <time.h>

#include <sys/socket.h>

#include <asm/types.h>
#include <libnetlink.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <arpa/inet.h>

#include "contiki.h"
#include "contiki-net.h"
#include "net/uip.h"
#include "net/rpl/rpl.h"
#include "net/uip-ds6.h"

#include "rpld.h"
#include "ethdev.h"
#include "prefix.h"
#include "table.h"

#define RTPROT_RPL     20

extern  uip_ds6_route_t uip_ds6_routing_table[];

static uint16_t dag_id[] = {0x1111, 0x1100, 0, 0, 0, 0, 0, 0x0011};
static struct interface *iface;
static struct rtnl_handle *rth;

enum {
  ROUTE_NODE_CREATED,
  ROUTE_NODE_UPDATED,
  ROUTE_NODE_MODIFIED,
  ROUTE_NODE_KERNEL
};

struct route_table *rt;

struct route_node_info {
  unsigned char metric;
  char status;
};

struct nlist {
  int seq;
   int err;
  struct nlist *next;
  struct nlist *parent;
};
static struct nlist nl;

/*---------------------------------------------------------------*/
/* sendmsg() to netlink socket then recvmsg() */
static int
netlink_talk(struct nlmsghdr *n, int len)
{
  int status;

  n->nlmsg_seq = ++nl.seq;
  /* Request an acknowledgement by setting NLM_F_ACK */
  n->nlmsg_flags |= NLM_F_ACK;

  status = rtnl_send(rth, (char *) n, len);
  if (status < 0) {
    fprintf(stderr, "netlink_talk rtnl_send() error: %s\n", strerror(errno));
  }

  // Insert here receive message
  return status;
}

/*---------------------------------------------------------------*/
static int
kernel_route_create(struct route_node *node)
{
  int bytelen;
  struct route_node_info *rni;

  struct {
    struct nlmsghdr n;
    struct rtmsg r;
    char buf[1024];
  } req;

  memset(&req, 0, sizeof(req));
  bytelen = 16;

  rni = node->aggregate;

  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  req.n.nlmsg_flags =  NLM_F_CREATE | NLM_F_REQUEST;
  req.n.nlmsg_type = RTM_NEWROUTE;
  req.r.rtm_family = node->p.family;
  req.r.rtm_dst_len = node->p.prefixlen;

  req.r.rtm_protocol = RTPROT_RPL;
  req.r.rtm_type = RTN_UNICAST;
  req.r.rtm_table = RT_TABLE_MAIN;
  req.r.rtm_scope = RT_SCOPE_LINK;

  addattr_l(&req.n, sizeof(req), RTA_DST, node->p.u.val, bytelen);
  addattr_l(&req.n, sizeof(req), RTA_GATEWAY, node->info, bytelen);
  addattr32(&req.n, sizeof(req), RTA_OIF, iface->ifindex);
  addattr32(&req.n, sizeof(req), RTA_PRIORITY, rni->metric);

  /* Talk to netlink socket */
  return netlink_talk(&req.n, req.n.nlmsg_len);
}
/*---------------------------------------------------------------*/
static int
kernel_route_delete(struct route_node *node)
{
  int bytelen;

  struct {
    struct nlmsghdr n;
    struct rtmsg r;
    char buf[1024];
  } req;

  memset(&req, 0, sizeof(req));
  bytelen = 16;

  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  req.n.nlmsg_flags =  NLM_F_CREATE | NLM_F_REQUEST;
  req.n.nlmsg_type = RTM_DELROUTE;
  req.r.rtm_family = node->p.family;
  req.r.rtm_dst_len = node->p.prefixlen;

  req.r.rtm_protocol = RTPROT_RPL;
  req.r.rtm_type = RTN_UNICAST;
  req.r.rtm_table = RT_TABLE_MAIN;
  req.r.rtm_scope = RT_SCOPE_LINK;

  addattr_l(&req.n, sizeof(req), RTA_DST, node->p.u.val, bytelen);
  addattr32(&req.n, sizeof(req), RTA_OIF, iface->ifindex);

  /* Talk to netlink socket */
  return netlink_talk(&req.n, req.n.nlmsg_len);
}

/*----------------------------------------------------------------------*/
static int
kernel_route_get(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
  FILE *fp = (FILE*)arg;
  struct rtmsg *r = NLMSG_DATA(n);
  int len = n->nlmsg_len;
  struct rtattr * tb[RTA_MAX+1];
  int index = -1;
  int metric = 0;
  void *dest = NULL;
  void *gate = NULL;

  struct route_node_info *rni;
  struct route_node *node;
  struct prefix p;

  if (n->nlmsg_type != RTM_NEWROUTE && n->nlmsg_type != RTM_DELROUTE) {
          fprintf(stderr, "Not a route: %08x %08x %08x\n",
                  n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
          return 0;
  }

  len -= NLMSG_LENGTH(sizeof(*r));
  if (len < 0) {
          fprintf(fp, "wrong NLSMG length %d", len);
          return -1;
  }

  memset(tb, 0, sizeof(tb));
  parse_rtattr(tb, RTA_MAX, RTM_RTA(r), len);

  if (tb[RTA_OIF]) {
    index = * (int *) RTA_DATA(tb[RTA_OIF]);
    iface = if_get_active();
    if (iface == NULL || iface->ifindex != index) {
      return 0;
    }
  }

  if (tb[RTA_DST]) {
    dest = RTA_DATA(tb[RTA_DST]);
  }

  if (tb[RTA_GATEWAY]) {
    gate = RTA_DATA(tb[RTA_GATEWAY]);
  }

  if (tb[RTA_PRIORITY]) {
    metric = *(int *) RTA_DATA(tb[RTA_PRIORITY]);
  }

  if (r->rtm_flags & RTM_F_CLONED) {
    return 0;
  }

  if (r->rtm_protocol != RTPROT_RPL) {
    return 0;
  }

  p.family = AF_INET6;
  p.prefixlen = r->rtm_dst_len;
  IPV6_ADDR_COPY(&p.u, dest);
  node = route_node_get(rt, &p);
  if (gate) {
    node->info = malloc(sizeof(struct in6_addr));
    IPV6_ADDR_COPY(node->info, gate);
  }
  node->aggregate = malloc(sizeof(struct route_node_info));
  rni = (struct route_node_info *) node->aggregate;
  rni->status = ROUTE_NODE_KERNEL;
  rni->metric = metric;
  route_node_lock(node);
  if(iface->verbose > 2) {
    char dst[INET6_ADDRSTRLEN], src[INET6_ADDRSTRLEN];

    inet_ntop(AF_INET6, dest, dst, INET6_ADDRSTRLEN);
    if (gate) {
      inet_ntop(AF_INET6, gate, src, INET6_ADDRSTRLEN);
    } else {
      memcpy(src, "unkown", strlen("unknown"));
    }
    fprintf(fp, "from kernel route %s/%d via %s\n", dst, p.prefixlen, src);
  }

  fflush(fp);
  return 0;

}

/*---------------------------------------------------------------*/
static void
kernel_route_update(void)
{
  struct route_node *node;
  struct route_node_info *ni;
  char addr[INET6_ADDRSTRLEN];
  int status = 0;

  for (node = route_top(rt); node != NULL; node = route_next(node)) {
    if (node->info == NULL)
      continue;
    inet_ntop(AF_INET6, &node->p.u.prefix6, addr, INET6_ADDRSTRLEN);
    ni = (struct route_node_info *) node->aggregate;
    if (ni == NULL) {
      fprintf(stderr, "network interface missing in the table\n");
      continue;
    }
    switch(ni->status) {
    case ROUTE_NODE_CREATED:
      if (iface->verbose > 2) {
        fprintf(stderr, "creating route node %s\n", addr);
      }
      status = kernel_route_create(node);
      break;
    case ROUTE_NODE_MODIFIED:
      if (iface->verbose > 2) {
        fprintf(stderr, "changing route node %s\n", addr);
      }
      status = kernel_route_delete(node);
      if (status < 0) {
        fprintf(stderr, "unrecoverable error\n");
        exit(1);
      }
      status = kernel_route_create(node);
      break;
    case ROUTE_NODE_KERNEL:
      if (iface->verbose > 2) {
        fprintf(stderr, "kernel route node %s\n", addr);
      }
      route_node_lock(node);
      break;
    default:
      break;
    }
    if (status < 0) {
      fprintf(stderr, "unrecoverable error\n");
      exit(1);
    }
    if ((node->lock == 0)) {
      if (iface->verbose > 2) {
        fprintf(stderr, "deleting route node %s\n", addr);
      }
      status = kernel_route_delete(node);
      if (status < 0) {
        fprintf(stderr, "unrecoverable error\n");
        exit(1);
      }
      free (node->info);
      node->info = NULL;
      route_node_delete(node);
    }
  }
}

/*---------------------------------------------------------------*/
static void
br_init(void)
{
  int fd;

  /* Initialize nl */
  memset(&nl, 0, sizeof(nl));

  /* Allocate route table */
  rt = route_table_init();
  rt->top = NULL;

  /* Create rtnetlink handle */
  rth = (struct rtnl_handle *) malloc (sizeof(struct rtnl_handle));
  if (rth == NULL) {
    perror("Cannot allocate memory");
    exit(errno);
  }

  fd = rtnl_open(rth, 0);
  if (fd < 0) {
    fprintf (stderr, "Cannot open rtnetlink.\n");
    free(rth);
    exit(errno);
  }
  if(rtnl_wilddump_request(rth, AF_INET6, RTM_GETROUTE) < 0) {
    perror("Cannot send dump request");
    free(rth);
    exit(errno);
  }
  if(rtnl_dump_filter(rth, (rtnl_filter_t) kernel_route_get,
      stderr, NULL, NULL) < 0) {
    fprintf(stderr, "Flush terminated\n");
    free(rth);
    exit(errno);
  }
}

/*---------------------------------------------------------------*/
static void
br_poll(void)
{
  uip_ds6_route_t *locroute;
  struct route_node *node;

  route_table_unlock(rt);
 for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB; locroute++) {
    if((locroute->isused)) {
      struct prefix p;
      struct route_node_info *rni;
      int new;

      new = 0;
      p.family = AF_INET6;
      p.prefixlen = locroute->length;
      IPV6_ADDR_COPY(&p.u, &locroute->ipaddr);

      node = route_node_lookup(rt, &p);
      if (node == NULL) {
        new = 1;
        node = route_node_get(rt, &p);
      }
      route_node_lock(node);

      if (new) {
        rni = (struct route_node_info *) malloc(sizeof(struct route_node_info));
        rni->metric = locroute->metric;
        rni->status = ROUTE_NODE_CREATED;
        if (iface->verbose > 2) {
          fprintf(stderr, "adding new route to table\n");
        }
        node->aggregate = rni;
        node->info = malloc(sizeof(struct in6_addr));
      }
      else {
        rni = (struct route_node_info *) node->aggregate;
        if (iface->verbose > 2) {
          fprintf(stderr, "route found in table, metric %d - ", rni->metric);
        }
        if (IPV6_ADDR_SAME(node->info, &locroute->nexthop)) {
          rni->status = ROUTE_NODE_UPDATED;
          if (iface->verbose > 2) {
            fprintf(stderr, "same next hop\n");
          }
        }
        else {
          rni->status = ROUTE_NODE_MODIFIED;
          if (iface->verbose > 2) {
            fprintf(stderr, "next hop modified\n");
          }
        }
      }

      IPV6_ADDR_COPY(node->info, &locroute->nexthop);

      if (iface->verbose > 2) {
        char src_addr[INET6_ADDRSTRLEN], dst_addr[INET6_ADDRSTRLEN];

        inet_ntop(AF_INET6, &node->p.u.prefix6, dst_addr, INET6_ADDRSTRLEN);
        inet_ntop(AF_INET6, node->info, src_addr, INET6_ADDRSTRLEN);
        fprintf(stderr, "route to %s/%d via %s",
                dst_addr, node->p.prefixlen, src_addr);
        if (iface->verbose > 3) {
          fprintf(stderr, ", metric:%d, lifetime:%us, saved lifetime:%us, learned from:%d",
              locroute->metric, locroute->state.lifetime, locroute->state.saved_lifetime,
              locroute->state.learned_from);
        }
        fprintf(stderr, "\n");
      }
    }
  }
  if (iface->verbose > 2) {
    route_node_dump(rt);
  }
  kernel_route_update();
}

/*------------------------------------------------------------------*/
static void
br_exit(void)
{
  fprintf(stderr, "Process exited.\n");
}

/*------------------------------------------------------------------*/

PROCESS(border_router_process, "RPL Border Router");

PROCESS_THREAD(border_router_process, ev, data)
{
  rpl_dag_t *dag;
  char buf[sizeof(dag_id)];
  uip_ipaddr_t ipaddr;

  PROCESS_POLLHANDLER(br_poll());
  PROCESS_EXITHANDLER(br_exit());

  PROCESS_BEGIN();

  iface = (struct interface *) data;

  /* Configure MAC address */
  memcpy(&uip_lladdr.addr, &iface->eui48, sizeof(uip_lladdr.addr));

  /* Configure global IPV6 address */
  if (iface->prefix) {
    memcpy(&ipaddr, iface->prefix, sizeof(struct in6_addr));
  }
  else {
    memcpy(&ipaddr, &iface->if_gaddr[0], sizeof(struct in6_addr));
  }

  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  /* Configure DAG id */
  if (iface->dagid) {
    memcpy(buf, iface->dagid, sizeof(struct in6_addr));
  }
  else {
    memcpy(buf, dag_id, sizeof(dag_id));
  }
//  if (iface->metric == 0) {
    /* Set up DODAG root */
    dag = rpl_set_root(RPL_DEFAULT_INSTANCE, (uip_ip6addr_t *)buf);
    /* Set up RPL prefix */
    rpl_set_prefix(dag, &ipaddr, 64);
//  }

  br_init();

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == ethnet_event);
    tcpip_input();
    br_poll();
  }

  PROCESS_END();
}
