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

struct config *config_new(const char *digest_name, size_t block_size, int workers)
{
    const EVP_MD *md;
    struct config *c;
    int err;

    assert(workers > 0);

    md = EVP_get_digestbyname(digest_name);
    if (md == NULL) {
        errno = EINVAL;
        return NULL;
    }

    c = malloc(sizeof(*c));
    if (c == NULL)
        return NULL;

    c->md = md;
    c->block_size = block_size;
    c->workers = workers;

    err = compute_zero_md(c);
    if (err) {
        config_free(c);
        errno = err;
        return NULL;
    }

    return c;
}

void config_free(struct config *c)
{
    free(c);
}
