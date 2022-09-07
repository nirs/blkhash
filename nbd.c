// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifdef HAVE_NBD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libnbd.h>

#include "blksum.h"

#define FAIL_NBD() FAIL("%s", nbd_get_error())

struct nbd_src {
    struct src src;
    struct nbd_handle *h;
};

struct extent_request {
    int64_t length;
    struct extent *extents;
    size_t count;
};

static ssize_t nbd_ops_pread(struct src *s, void *buf, size_t len, int64_t offset)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    int res = -1;

    if (offset + len > s->size)
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
                            uint64_t offset, uint32_t *entries,
                            size_t nr_entries, int *error)
{
    struct extent_request *r = user_data;
    size_t count = nr_entries / 2;
    int64_t reply_length = 0;
    size_t i;

    if (strcmp(metacontext, LIBNBD_CONTEXT_BASE_ALLOCATION) != 0) {
        DEBUG("unexpected meta context: %s", metacontext);
        return 0;
    }

    if (*error) {
        DEBUG("extent callback failed error=%d", *error);
        return 0;
    }

    r->extents = malloc(count * sizeof(*r->extents));
    if (r->extents == NULL)
        FAIL_ERRNO("malloc");

    /*
     * NBD protocol allows the last request to end after the specified
     * range. This is bad for our use case so we clip the last extent to
     * the specified range.
     *
     * To be more robust against incompliant servers stop iteration once we
     * reach the requested range, so we may report less extents.
     */

    for (i = 0; i < count && reply_length < r->length; i++) {
        uint32_t length = entries[i * 2];
        uint32_t flags = entries[i * 2 + 1];

        if (reply_length + length > r->length) {
            length = r->length - reply_length;
        }

        reply_length += length;
        r->extents[i].length = length;
        r->extents[i].zero = (flags & LIBNBD_STATE_ZERO) != 0;
    }

    r->count = i;

    return 0;
}

static int nbd_ops_extents(struct src *s, int64_t offset, int64_t length,
                           struct extent **extents, size_t *count)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    struct extent_request r = { .length=length };
    nbd_extent_callback cb = { .callback=extent_callback, .user_data=&r, };

    if (!ns->src.can_extents)
        return -1;

    if (nbd_block_status(ns->h, length, offset, cb, 0)) {
        /*
         * Extents are a performance optimization, we can compute
         * checksum without extents, slower.
         */
        DEBUG("%s", nbd_get_error());
        free(r.extents);
        ns->src.can_extents = false;
        return -1;
    }

    /* Caller need to free extents. */
    *extents = r.extents;
    *count = r.count;

    /*
     * According to nbd_block_status(3), the extent callback may not be
     * called at all if the server does not support base:allocation, or
     * could not retrun the requested data for some reason. Treat this
     * as a temporary error so caller can use a fallback. Hopefully the
     * next call would succeed.
     */
    return r.count > 0 ? 0 : -1;
}

static int nbd_ops_aio_pread(struct src *s, void *buf, size_t len,
                             int64_t offset, completion_callback cb,
                             void *user_data)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    int64_t res;

    if (offset + len > s->size)
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

static int nbd_ops_aio_run(struct src *s, int timeout)
{
    struct nbd_src *ns = (struct nbd_src *)s;
    int res;
    int in_flight;

    in_flight = nbd_aio_in_flight(ns->h);

    if (in_flight == 0)
        return 1;

    /*
     * Testing shows that we need to poll 1-7 times before a command
     * complete.
     */
    do {
        res = nbd_poll(ns->h, timeout);
    } while (res == 1 && nbd_aio_in_flight(ns->h) >= in_flight);

    if (res == -1)
        FAIL_NBD();

    return res;
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
    .aio_run = nbd_ops_aio_run,
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
