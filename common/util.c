// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#define _GNU_SOURCE     /* For O_DIRECT */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if __APPLE__
#include <mach/mach_time.h> /* mach_absolute_time */
#endif

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
#if __APPLE__
    static int have_abstime = -1;
    static double usec_per_abstime;

    switch (have_abstime) {
    case -1: {
        mach_timebase_info_data_t info;
        if (mach_timebase_info(&info) != KERN_SUCCESS) {
            have_abstime = 0;
            break;
        }

        have_abstime = 1;
        usec_per_abstime = (double)info.numer / info.denom / 1000;
    }
    case 1:
        return mach_absolute_time() * usec_per_abstime;
    }

#elif defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * MICROSECONDS + ts.tv_nsec / 1000;
#endif

    /* Safe fallback. */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * MICROSECONDS + tv.tv_usec;
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

const EVP_MD *create_digest(const char *name)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    /*
     * Aovids implicit fetching on each call to EVP_DigestInit_ex()
     * https://www.openssl.org/docs/man3.1/man7/crypto.html#Explicit-fetching.
     */
    return EVP_MD_fetch(NULL, name, NULL);
#else
    if (strcmp(name, "null") == 0)
        return EVP_md_null();

    return EVP_get_digestbyname(name);
#endif
}

void free_digest(const EVP_MD *md)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD_free((EVP_MD *)md);
#else
    (void)md;
#endif
}
