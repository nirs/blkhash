// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "blksum.h"
#include "util.h"

#define WIDTH 50

struct progress {
    int64_t size;
    int64_t done;
    int value;
};

static struct progress progress;

static inline void draw_bar(char *buf, int done, int size)
{
    assert(done <= size);

    memset(buf, '|', done);
    memset(buf + done, ' ', size - done);
    buf[size] = 0;
}

static inline void progress_draw()
{
    char bar[WIDTH + 1];

    draw_bar(bar, progress.value * WIDTH / 100, WIDTH);
    fprintf(stdout, " %3d%% [%s]    \r", progress.value, bar);
    fflush(stdout);
}

void progress_init(int64_t size)
{
    progress.size = size;
    progress.done = 0;
    progress.value = -1;
}

void progress_update(int64_t len)
{
    int value;

    progress.done = MIN(progress.done + len, progress.size);

    value = (double)progress.done / (double)progress.size * 100;

    if (value > progress.value) {
        progress.value = value;
        progress_draw();
    }
}

void progress_clear()
{
    char space[80];

    memset(space, ' ', sizeof(space) - 1);
    space[sizeof(space) - 1] = 0;

    fprintf(stdout, "%s\r", space);
    fflush(stdout);
}
