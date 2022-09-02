// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "blksum.h"
#include "util.h"

#define WIDTH 50

struct progress *progress_open(int64_t size)
{
    struct progress *p;

    if (!isatty(fileno(stdout)))
        return NULL;

    p = calloc(1, sizeof(*p));
    if (p == NULL)
        FAIL_ERRNO("calloc");

    p->size = size;
    p->value = -1;

    return p;
}

static inline void draw_bar(char *buf, int done, int size)
{
    assert(done <= size);

    memset(buf, '|', done);
    memset(buf + done, ' ', size - done);
    buf[size] = 0;
}

static inline void progress_draw(struct progress *p)
{
    char bar[WIDTH + 1];

    draw_bar(bar, p->value * WIDTH / 100, WIDTH);
    fprintf(stdout, " %3d%% [%s]    \r", p->value, bar);
    fflush(stdout);
}

void progress_update(struct progress *p, int64_t len)
{
    int value;

    p->done = MIN(p->done + len, p->size);

    value = (double)p->done / p->size * 100;

    if (value > p->value) {
        p->value = value;
        progress_draw(p);
    }
}

void progress_close(struct progress *p)
{
    char space[80];

    if (p == NULL)
        return;

    memset(space, ' ', sizeof(space) - 1);
    space[sizeof(space) - 1] = 0;

    fprintf(stdout, "%s\r", space);
    fflush(stdout);

    free(p);
}
