// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#define _GNU_SOURCE     /* For O_DIRECT */

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

#define MICROSECONDS 1000000

void format_hex(unsigned char *md, unsigned int len, char *s)
{
    for (int i = 0; i < len; i++) {
        snprintf(&s[i * 2], 3, "%02x", md[i]);
    }
    s[len * 2] = 0;
}

/* Based on nbdkit stats filter. */
char* humansize(int64_t bytes)
{
    int r;
    char *s;

    if (bytes < KiB)
        r = asprintf(&s, "%" PRIu64 " bytes", bytes);
    else if (bytes < MiB)
        r = asprintf(&s, "%.2f KiB", bytes / (double)KiB);
    else if (bytes < GiB)
        r = asprintf(&s, "%.2f MiB", bytes / (double)MiB);
    else if (bytes < TiB)
        r = asprintf(&s, "%.2f GiB", bytes / (double)GiB);
    else
        r = asprintf(&s, "%.2f TiB", bytes / (double)TiB);

    return r != -1 ? s : NULL;
}

uint64_t gettime(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * MICROSECONDS + ts.tv_nsec / 1000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * MICROSECONDS + tv.tv_usec;
#endif
}

bool supports_direct_io(const char *filename)
{
#ifdef O_DIRECT
    int fd = open(filename, O_RDONLY | O_DIRECT);
    if (fd != -1)
        close(fd);
    return fd != -1;
#else
    return false;
#endif
}
