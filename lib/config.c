// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "blkhash-internal.h"
#include "util.h"

static int compute_zero_md(struct config *c)
{
    unsigned char *buf;
    int err = 0;

    buf = calloc(1, c->block_size);
    if (buf == NULL)
        return errno;

    /* Returns 1 on success. */
    if (!EVP_Digest(buf, c->block_size, c->zero_md, &c->md_len, c->md, NULL))
        err = ENOMEM;

    free(buf);
    return err;
}

int config_init(struct config *c, const struct blkhash_opts *opts)
{
    c->md = lookup_digest(opts->digest_name);
    if (c->md == NULL)
        return EINVAL;

    c->block_size = opts->block_size;
    c->workers = opts->threads;
    c->streams = opts->threads;

    return compute_zero_md(c);
}
