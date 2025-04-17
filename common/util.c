// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#define _GNU_SOURCE     /* For O_DIRECT */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

#define MICROSECONDS 1000000

void format_hex(unsigned char *md, unsigned int len, char *s)
{
    for (unsigned i = 0; i < len; i++) {
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

int64_t parse_humansize(const char *s)
{
    char *end;
    double value;

    value = strtod(s, &end);

    if (value < 0)
        return -EINVAL; /* size must be positive. */

    if (end == s)
        return -EINVAL; /* nothing was parsed. */

    if (*end == '\0')
        return value; /* no prefix, value in bytes. */
    else if (strcmp(end, "k") == 0)
        return value * KiB;
    else if (strcmp(end, "m") == 0)
        return value * MiB;
    else if (strcmp(end, "g") == 0)
        return value * GiB;
    else if (strcmp(end, "t") == 0)
        return value * TiB;
    else
        return -EINVAL; /* Unknown suffix. */
}

uint64_t gettime(void)
{
    struct timespec ts;

#if __APPLE__
    /*
     * clock that increments monotonically, in the same manner as
     * CLOCK_MONOTONIC_RAW, but that does not increment while the system is
     * asleep.  The returned value is identical to the result of
     * mach_absolute_time() after the appropriate mach_timebase conversion is
     * applied.
     */
    clockid_t clock_id = CLOCK_UPTIME_RAW;
#elif defined (__linux__)
    /*
     * Similar to CLOCK_MONOTONIC, but provides access to a raw hardware-based
     * time that is not subject to NTP adjustments or the incremental
     * adjustments performed by adjtime(3).
     */
    clockid_t clock_id = CLOCK_MONOTONIC_RAW;
#elif defined (__FreeBSD__)
    /*
     * Starts at zero when the kernel boots and increments monotonically in	SI
     * seconds while the machine is	running.
     */
    clockid_t clock_id = CLOCK_UPTIME;
#else
    /* Safe default. */
    clockid_t clock_id = CLOCK_MONOTONIC;
#endif

    clock_gettime(clock_id, &ts);
    return (uint64_t)ts.tv_sec * MICROSECONDS + ts.tv_nsec / 1000;
}

bool supports_direct_io(const char *filename)
{
/* QEMU uses O_DSYNC if O_DIRECT isn't available. */
#ifndef O_DIRECT
#define O_DIRECT O_DSYNC
#endif
    int fd = open(filename, O_RDONLY | O_DIRECT);
    if (fd != -1)
        close(fd);
    return fd != -1;
}
