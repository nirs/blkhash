// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "src.h"

struct pipe_src {
    struct src src;
    int fd;
};

static ssize_t pipe_ops_read(struct src *s, void *buf, size_t len)
{
    struct pipe_src *ps = (struct pipe_src *)s;
    size_t pos = 0;

    while (pos < len) {
        ssize_t n;

        n = read(ps->fd, buf + pos, len - pos);

        if (n == -1) {
            if (errno == EINTR && !running())
                /* Emulate end of file. */
                break;

            FAIL_ERRNO("read");
        }

        if (n == 0)
            /* End of file. */
            break;

        pos += n;
    }

    return pos;
}

static void pipe_ops_close(struct src *s)
{
    struct pipe_src *ps = (struct pipe_src *)s;

    /* fd owned by caller, not closing. */
    free(ps);
}

static struct src_ops pipe_ops = {
    .read = pipe_ops_read,
    .close = pipe_ops_close,
};

struct src *open_pipe(int fd)
{
    struct pipe_src *ps;

    ps = calloc(1, sizeof(*ps));
    if (ps == NULL)
        FAIL_ERRNO("calloc");

    ps->src.ops = &pipe_ops;
    ps->src.size = -1;
    ps->fd = fd;

    return &ps->src;
}
