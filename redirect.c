/*
 * redirect.c
 *
 *  Created on: 31 Jul 2012
 *      Author: zramaros
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <syslog.h>

static char const *priov[] = {
"EMERG:", "ALERT:", "CRIT:", "ERR:", "WARNING:", "NOTICE:", "INFO:", "DEBUG:"
};

static size_t writer(void *cookie, char const *data, size_t leng)
{
    (void)cookie;
    int p = LOG_DEBUG, len;
    do len = strlen(priov[p]);
    while (memcmp(data, priov[p], len) && --p >= 0);

    if (p < 0) p = LOG_INFO;
    else data += len, leng -= len;
    while (*data == ' ') ++data, --leng;

    syslog(p, "%.*s", leng, data);
    return  leng;
}

static int noop(void) {return 0;}
static cookie_io_functions_t log_fns = {
    (void*) noop, (void*) writer, (void*) noop, (void*) noop
};

void tolog(FILE **pfp)
{
    setvbuf(*pfp = fopencookie(NULL, "w", log_fns), NULL, _IOLBF, 0);
}
