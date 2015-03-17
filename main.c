/*
 * rpld.c
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include "contiki.h"
#include "contiki-net.h"
#include "net/uip.h"
#include "net/rpl/rpl.h"
#include "net/uip-ds6.h"

#include <arpa/inet.h>

#include "ethdev.h"
#include "sundev.h"
#include "rpld.h"

char *progname;
char  progbuf[PATH_MAX];

PROCINIT(&tcpip_process);

static struct option const longopts[] =
{
    { "help",      0, 0, '?'},
    { "interface", 0, 0, 'i'},
    { "prefix",    1, NULL, 'p'},
    { "dagid",     1, NULL, 'd'},
    { "rank",      1, NULL, 'r'},
    { "verbose",   0, 0, 'v'},
    { "daemon",    0, 0, 'D'},
    { name: 0 },
};

static void
usage(void)
{
  fprintf (stderr, "Usage: %s -i ifname\n", progname);
  fprintf (stderr, "%s%s[-v] [--verbose]                 verbose\n", progbuf, progbuf);
  fprintf (stderr, "%s%s[-p prefix] [--prefix prefix]    announce this IPv6 prefix to the interface\n", progbuf, progbuf);
  fprintf (stderr, "%s%s[-d dagid] [--dagid dagid]       DAG ID to use\n", progbuf, progbuf);
//  fprintf (stderr, "%s%s[-r rank] [--rank rank]          initial rank to announce\n", progbuf, progbuf);
  fprintf (stderr, "%s%s[?] [--help]                     print this help\n", progbuf, progbuf);
  fprintf (stderr, "%s%s[-D] [--daemon]                  run in background\n", progbuf, progbuf);
}

int
main(int argc, char *argv[])
{
  int len;
  int i;
  char ch;
  char *e;
  struct interface *iface;
  struct netdrv *netdrv;

  char *iface_name;
  char *dagid;
  char *prefix;
  int rank;
  int instanceid;
  int interval;
  int verbose;
  int daemon;

  /* Our process ID and Session ID */
  pid_t pid, sid;

  /*
   * Set line buffering for file descriptors so we see stdout and stderr
   * properly interleaved.
   */
  setvbuf(stdout, (char*)NULL, _IOLBF, 0);
  setvbuf(stderr, (char*)NULL, _IOLBF, 0);

  progname = strrchr(argv[0],'/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  len = strlen(progname) + 2;
  for (i=0; i<len; i++)
    progbuf[i] = ' ';
  progbuf[i] = 0;

  /*
   * check for no arguments
   */
  if (argc == 1) {
    usage();
    return 0;
  }

  interval = 0;
  verbose = 0;
  daemon = 0;
  rank = 0;
  iface = NULL;
  iface_name = NULL;
  dagid = NULL;
  prefix = NULL;

  /*
   * process command line arguments
   */
  while ((ch = getopt_long(argc,argv,"?hd:i:p:vD", longopts, 0)) != -1) {

    switch (ch) {
    case 'i':   /* interface name */
      iface_name = optarg;
      break;
    case 'p':   /* prefix */
      prefix = optarg;
      break;
    case 'd':   /* dag id */
      dagid = optarg;
      break;
    case 'r':   /* rank */
      rank = strtol(optarg, &e, 0);
      if ((e == optarg) || (*e != 0)) {
        fprintf (stderr, "%s: invalid rank specified '%s'\n", progname, optarg);
        return 1;
      }
      break;
    case 'v':
      verbose++;
      break;
    case 'D':
      daemon++;
      break;
    case '?':
    case 'h':
    default:
      usage();
      return 0;
    }
  }

  if (iface_name == NULL) {
    usage();
    return 0;
  }

  /* Let's run the process as a daemon */
  if(daemon) {
    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
     * we can exit the parent process
     */
    if (pid > 0) {
      exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid <0) {
      exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
      exit(EXIT_FAILURE);
    }
    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);

    /* Redirect stderr to syslog */
    tolog(&stderr);
  }

  scan_net_devices(verbose);

  iface = if_get_by_name(iface_name);
  if (iface == NULL) {
    fprintf(stderr, "Can not find interface %s\n", iface_name);
    return 0;
  }

  iface->verbose = verbose;
  iface->flags |= RPLD_FLAGS;

  if (dagid) {
    iface->dagid = (struct in6_addr *) malloc(sizeof(struct in6_addr));
    if (inet_pton(AF_INET6, dagid, iface->dagid) == 0) {
      fprintf(stderr, "%s is not a valid RPL DAG ID\n", dagid);
      return 0;
    }
    fprintf(stderr, "DAG id = %s\n", dagid);
  }

  if (prefix) {
    iface->prefix = (struct in6_addr *) malloc(sizeof(struct in6_addr));
    if (inet_pton(AF_INET6, prefix, iface->prefix) == 0) {
      fprintf(stderr, "%s is not a valid IPv6 prefix\n", prefix);
      return 0;
    }
    fprintf(stderr, "prefix %s\n", prefix);
  }

  if (rank) {
    iface->metric = rank;
  }

  if (strncmp(iface->name, "tap", 3) == 0) {
    netdrv = &sundrv;
  }
  else {
    netdrv = &ethdrv;
  }
  if (netdrv->init(iface) < 0) {
    fprintf(stderr, "can't set up %s\n", iface_name);
    return 0;
  }

  if (verbose > 2) {
    fprintf(stderr, "interface index = %d\n", iface->ifindex);
  }

  /* Init clock */
  clock_init();

  /* Init process */
  process_init();

  /* procinit_init initializes RPL which sets a ctimer for the first DIS */
  /* We must start etimers and ctimers,before calling it */
  process_start(&etimer_process, NULL);
  ctimer_init();

  /* Start border router process */
  process_start(&border_router_process, (void *) iface);
  fprintf(stderr, "Border Router Process started\n");

  /* netdrv.setup starts the network process and sets the network output function */
  /* We must start the border router process before calling it */
  netdrv->setup();

  procinit_init();

  while (1) {
    process_run();
    etimer_request_poll();
  }

  return 0;
}
