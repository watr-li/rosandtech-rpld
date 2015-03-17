/*
 * sundev.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * Authors: Zafi Ramarosandratana (Rosand Technologies)
 *
 */


#ifndef _PIPEDEV_H
#define _PIPEDEV_H

#include "netdrv.h"

#define SOCK_PATH                "/tmp/rpld-sock"

extern struct netdrv sundrv;
extern process_event_t ethnet_event;

PROCESS_NAME(sundev_process);

#endif /* _PIPEDEV_H */
