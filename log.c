// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "blksum.h"

static pthread_mutex_t failed = PTHREAD_MUTEX_INITIALIZER;

void fail(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    /*
     * Unless we run in debug mode, only the first thread will log the
     * failure message.
     */
    if (pthread_mutex_trylock(&failed) == 0 || debug)
        vfprintf(stderr, fmt, args);

    va_end(args);

    exit(EXIT_FAILURE);
}
