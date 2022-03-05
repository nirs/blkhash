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
    p->value = -1;

    return p;
}

void progress_update(struct progress *p, size_t n)
{
    int err;

    err = pthread_mutex_lock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_lock: %s", strerror(err));

    p->done += n;

    err = pthread_mutex_unlock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_unlock: %s", strerror(err));
}

static inline int get_value(struct progress *p)
{
    size_t done;
    int err;

    err = pthread_mutex_lock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_lock: %s", strerror(err));

    done = p->done;

    err = pthread_mutex_unlock(&p->mutex);
    if (err)
        FAIL("pthread_mutex_unlock: %s", strerror(err));

    if (done > p->count)
        done = p->count;

    return done * 100 / p->count;
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
    char bar[WIDTH + 1];
    int value = get_value(p);

    if (value > p->value) {
        p->value = value;
        draw_bar(bar, value * WIDTH / 100, WIDTH);
        fprintf(stdout, " %3d%% [%s]    \r", value, bar);
        fflush(stdout);
    }

    return value < 100;
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
