// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "blkhash-internal.h"
#include "digest.h"
#include "util.h"

static int compute_zero_md(struct config *c)
{
    unsigned char *buf;
    struct digest *digest = NULL;
    int err;

    buf = calloc(1, c->block_size);
    if (buf == NULL)
        return errno;

    err = -digest_create(c->digest_name, &digest);
    if (err)
        goto out;

    err = -digest_init(digest);
    if (err)
        goto out;

    err = -digest_update(digest, buf, c->block_size);
    if (err)
        goto out;

    err = -digest_final(digest, c->zero_md, &c->md_len);

out:
    digest_destroy(digest);
    free(buf);

    return err;
}

int config_init(struct config *c, const struct blkhash_opts *opts)
{
    c->digest_name = opts->digest_name;
    c->block_size = opts->block_size;
    c->workers = opts->threads;
    c->streams = opts->streams;
    c->queue_depth = opts->queue_depth;

    /* XXX Initial value, needs testing */
    c->max_submissions = MAX(MAX(c->queue_depth, c->workers) * 4, 32);

    return compute_zero_md(c);
}
