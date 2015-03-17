/*
 * ethdev.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version
 * 2 of the Licence, or (at your option) any later version.
 *
 * Authors: Zafi Ramarosandratana (Rosand Technologies)
 *
 */


#ifndef _ETHDEV_H
#define _ETHDEV_H

#include "netdrv.h"

extern struct netdrv ethdrv;
extern process_event_t ethnet_event;

PROCESS_NAME(ethdev_process);

#endif /* _ETHDEV_H */
