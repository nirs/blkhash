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

#include <stdio.h>
#include <stdlib.h>

#include <libnbd.h>

#include "blksum.h"

struct nbd_src {
    struct src src;
    struct nbd_handle *h;
};

static ssize_t nbd_ops_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    int res = -1;

    if (offset + len > s->size) {
        fprintf(stderr, "read after end of nbd");
        exit(1);
    }

    res = nbd_pread(ns->h, buf, len, offset, 0);
    if (res == -1) {
        fprintf(stderr, "%s\n", nbd_get_error());
        exit(1);
    }

    return len;
}

static void nbd_ops_close(struct src *s)
{
    struct nbd_src *ns = (struct nbd_src *)s;

    nbd_shutdown(ns->h, LIBNBD_SHUTDOWN_ABANDON_PENDING);
    nbd_close(ns->h);

    free(ns);
}

static struct src_ops nbd_ops = {
    .pread = nbd_ops_pread,
    .close = nbd_ops_close,
};

struct src *open_nbd(const char *uri)
{
    struct nbd_handle *h;
    struct nbd_src *ns;

    h = nbd_create();
    if (h == NULL) {
        perror("nbd_create");
        exit(1);
    }

    if (nbd_connect_uri(h, uri)) {
        perror("nbd_connect_uri");
        exit(1);
    }

    ns = calloc(1, sizeof(*ns));
    if (ns == NULL)  {
        perror("calloc");
        exit(1);
    }

    ns->src.ops = &nbd_ops;
    ns->src.size = nbd_get_size(h);
    ns->h = h;

    return &ns->src;
}
