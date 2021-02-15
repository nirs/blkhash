/*
 * blkhash - block based hash
 * Copyright Nir Soffer <nirsof@gmail.com>.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "blksum.h"

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

        do {
            n = read(ps->fd, buf + pos, len - pos);
        } while (n == -1 && errno == EINTR);

        if (n < 0) {
            perror("read");
            exit(1);
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
    if (ps == NULL) {
        perror("calloc");
        exit(1);
    }

    ps->src.ops = &pipe_ops;
    ps->src.size = -1;
    ps->fd = fd;

    return &ps->src;
}
