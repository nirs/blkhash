// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "blkhash_internal.h"

static int compute_zero_md(struct config *c)
{
    unsigned char *buf;
    int err = 0;

    buf = calloc(1, c->block_size);
    if (buf == NULL)
        return errno;

    /* Returns 1 on success. */
    if (!EVP_Digest(buf, c->block_size, c->zero_md, &c->zero_md_len, c->md, NULL))
        err = ENOMEM;

    free(buf);
    return err;
}

int config_init(struct config *c, const char *digest_name, size_t block_size, int workers)
{
    assert(workers > 0);

    c->md = EVP_get_digestbyname(digest_name);
    if (c->md == NULL)
        return EINVAL;

    c->block_size = block_size;
    c->workers = workers;

    return compute_zero_md(c);
}
