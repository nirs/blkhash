// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blkhash-config.h"
#ifdef HAVE_NBD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libnbd.h>

#include "src.h"

#define FAIL_NBD() FAIL("%s", nbd_get_error())

struct nbd_src {
    struct src src;
    struct nbd_handle *h;
};

struct extent_request {
    struct extent *extents;
    size_t count;
    bool completed;
};

static ssize_t nbd_ops_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    int res = -1;

    if (offset + (int64_t)len > s->size)
        FAIL("read after end of file offset=%ld len=%ld size=%ld",
             offset, len, s->size);

    res = nbd_pread(ns->h, buf, len, offset, 0);
    if (res == -1)
        FAIL_NBD();

    return len;
}

/*
 * Extent callback should return -1 if it failed, and set *errror. Other
 * return values are ignored, unlike completion callback.
 */
static int extent_callback (void *user_data, const char *metacontext,
                            uint64_t offset __attribute__ ((unused)),
                            uint32_t *entries, size_t nr_entries, int *error)
{
    struct extent_request *r = user_data;
    size_t i;

    /* Limit result by size of extents array. If the server returned more
     * extents, we drop them. We will get them in the next request. */
    size_t count = MIN(nr_entries / 2, r->count);

    if (strcmp(metacontext, LIBNBD_CONTEXT_BASE_ALLOCATION) != 0) {
        DEBUG("unexpected meta context: %s", metacontext);
        return 0;
    }

    if (*error) {
        DEBUG("extent callback failed error=%d", *error);
        return 0;
    }

    for (i = 0; i < count; i++) {
        uint32_t length = entries[i * 2];
        uint32_t flags = entries[i * 2 + 1];

        r->extents[i].length = length;
        r->extents[i].zero = (flags & LIBNBD_STATE_ZERO) != 0;
    }

    r->count = count;
    r->completed = true;

    return 0;
}

static int nbd_ops_extents(struct src *s, int64_t offset, int64_t length,
                           struct extent *extents, size_t *count)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    struct extent_request r = { .extents=extents, .count=*count, };
    nbd_extent_callback cb = { .callback=extent_callback, .user_data=&r, };

    if (!ns->src.can_extents)
        return -1;

    if (nbd_block_status(ns->h, length, offset, cb, 0)) {
        /*
         * Extents are a performance optimization, we can compute
         * checksum without extents, slower.
         */
        DEBUG("%s", nbd_get_error());
        ns->src.can_extents = false;
        return -1;
    }

    /*
     * According to nbd_block_status(3), the extent callback may not be
     * called at all if the server does not support base:allocation, or
     * could not retrun the requested data for some reason. Treat this
     * as a temporary error so caller can use a fallback. Hopefully the
     * next call would succeed.
     */
    if (!r.completed)
        return -1;

    *count = r.count;
    return 0;
}

static int nbd_ops_aio_pread(struct src *s, void *buf, size_t len,
                             int64_t offset, completion_callback cb,
                             void *user_data)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    int64_t res;

    if (offset + (int64_t)len > s->size)
        FAIL("read after end of file offset=%ld len=%ld size=%ld",
             offset, len, s->size);

    res = nbd_aio_pread(
        ns->h, buf, len, offset,
        (nbd_completion_callback) {
            .callback=cb,
            .user_data=user_data,
        },
        0);

    if (res < 0)
        FAIL_NBD();

    return 0;
}

static int nbd_ops_aio_prepare(struct src *s, struct pollfd *pfd)
{
    struct nbd_src *ns = (struct nbd_src *)s;

    /* If we don't have in flight commands, polling on the socket will
     * block forever - disable polling in for this iteration. This is
     * an expected condition when we finished reading. */
    if (nbd_aio_in_flight(ns->h) == 0) {
        pfd->fd = -1;
        return 0;
    }

    pfd->fd = nbd_aio_get_fd(ns->h);

    switch (nbd_aio_get_direction(ns->h)) {
    case LIBNBD_AIO_DIRECTION_READ:
        pfd->events = POLLIN;
        break;
    case LIBNBD_AIO_DIRECTION_WRITE:
        pfd->events = POLLOUT;
        break;
    case LIBNBD_AIO_DIRECTION_BOTH:
        pfd->events = POLLIN | POLLOUT;
        break;
    default:
        /* Based on libnbd/poll.c this should never happen. */
        ERROR("Nothing to poll for in-flight requests");
        return -1;
    }

    return 0;
}

static int nbd_ops_aio_notify(struct src *s, struct pollfd *pfd)
{
    struct nbd_src *ns = (struct nbd_src *)s;

    /*
     * Based on libnbd/lib/poll.c, we need to prefer read over write,
     * and avoid invoking both notify_read() and notify_write(), since
     * notify_read() may change the state of the handle.
     *
     * We notify read on POLLHUP since nbd_poll does the same and it
     * works so far.
     */

    if (pfd->revents & (POLLIN | POLLHUP)) {
        return nbd_aio_notify_read(ns->h);
    } else if (pfd->revents & POLLOUT) {
        return nbd_aio_notify_write(ns->h);
    } else if (pfd->revents & (POLLERR | POLLNVAL)) {
        ERROR("NBD server closed the connection unexpectedly");
        return -1;
    }

    return 0;
}

static void nbd_ops_close(struct src *s)
{
    struct nbd_src *ns = (struct nbd_src *)s;

    DEBUG("Closing NBD %s", ns->src.uri);

    nbd_shutdown(ns->h, 0);
    nbd_close(ns->h);

    free(ns);
}

static struct src_ops nbd_ops = {
    .pread = nbd_ops_pread,
    .extents = nbd_ops_extents,
    .aio_pread = nbd_ops_aio_pread,
    .aio_prepare = nbd_ops_aio_prepare,
    .aio_notify = nbd_ops_aio_notify,
    .close = nbd_ops_close,
};

struct src *open_nbd(const char *uri)
{
    struct nbd_handle *h;
    struct nbd_src *ns;

    h = nbd_create();
    if (h == NULL)
        FAIL_NBD();

#if LIBNBD_HAVE_NBD_SET_PREAD_INITIALIZE
    /* Disable uneeded memset() before pread. */
    if (nbd_set_pread_initialize(h, false))
        FAIL_NBD();
#endif

    if (nbd_add_meta_context(h, LIBNBD_CONTEXT_BASE_ALLOCATION))
        FAIL_NBD();

    DEBUG("Opening NBD URI %s", uri);

    if (nbd_connect_uri(h, uri))
        FAIL_NBD();

    ns = calloc(1, sizeof(*ns));
    if (ns == NULL)
        FAIL_ERRNO("calloc");

    ns->src.ops = &nbd_ops;
    ns->src.uri = uri;
    ns->src.size = nbd_get_size(h);
    ns->src.can_extents = nbd_can_meta_context(
        h, LIBNBD_CONTEXT_BASE_ALLOCATION) > 0;
    ns->h = h;

    return &ns->src;
}

#endif /* HAVE_NBD */
