// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "blksum.h"

#define WIDTH 50

struct progress *progress_open(size_t count)
{
    struct progress *p;
    int err;

    if (!isatty(fileno(stdout)))
        return NULL;

    p = calloc(1, sizeof(*p));
    if (p == NULL)
        FAIL_ERRNO("calloc");

    err = pthread_mutex_init(&p->mutex, NULL);
    if (err)
        FAIL("pthread_mutex_init: %s", strerror(err));

    p->count = count;

    return p;
}

void progress_update(struct progress *p, size_t n)
{
    int err;

    err = pthread_mutex_lock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_lock: %s", strerror(err));

    p->done += n;

    if (p->done > p->count)
        p->done = p->count;

    err = pthread_mutex_unlock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_unlock: %s", strerror(err));
}

static inline void draw_bar(char *buf, int done, int size)
{
    assert(done <= size);

    memset(buf, '=', done);
    memset(buf + done, '-', size - done);
    buf[size] = 0;
}

bool progress_draw(struct progress *p)
{
    size_t done;
    double progress;
    char bar[WIDTH + 1];
    int err;

    err = pthread_mutex_lock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_lock: %s", strerror(err));

    done = p->done;

    err = pthread_mutex_unlock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_unlock: %s", strerror(err));

    progress = (double)done / p->count;

    draw_bar(bar, progress * WIDTH, WIDTH);

    fprintf(stdout, "  %6.2f%%  [%s]    \r", progress * 100.0, bar);
    fflush(stdout);

    return done < p->count;
}

void progress_close(struct progress *p)
{
    char space[80];
    int err;

    if (p == NULL)
        return;

    memset(space, ' ', sizeof(space) - 1);
    space[sizeof(space) - 1] = 0;

    fprintf(stdout, "%s\r", space);
    fflush(stdout);

    err = pthread_mutex_destroy(&p->mutex);
    if (err)
        FAIL("pthread_mutex_destroy: %s", strerror(err));

    free(p);
}
