// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdlib.h>

#include "blkhash-internal.h"

static inline void set_error(struct stream *s, int error)
{
    /* Keep the first error. */
    if (s->error == 0)
        s->error = error;
}

static int add_zero_blocks_before(struct stream *s, struct block *b)
{
    int64_t index;

    /* Don't modify the hash after errors. */
    if (s->error)
        return -1;

    index = s->last_index + s->config->streams;

    while (index < b->index) {
        if (!EVP_DigestUpdate(s->root_ctx, s->config->zero_md, s->config->md_len)) {
            set_error(s, ENOMEM);
            return -1;
        }
        s->last_index = index;
        index += s->config->streams;
    }

    return 0;
}

static int add_data_block(struct stream *s, struct block *b)
{
    unsigned char block_md[EVP_MAX_MD_SIZE];

    /* Don't modify the hash after errors. */
    if (s->error)
        return -1;

    if (!EVP_DigestInit_ex(s->block_ctx, s->config->md, NULL))
        goto error;

    if (!EVP_DigestUpdate(s->block_ctx, b->data, b->len))
        goto error;

    if (!EVP_DigestFinal_ex(s->block_ctx, block_md, NULL))
        goto error;

    if (!EVP_DigestUpdate(s->root_ctx, block_md, s->config->md_len))
        goto error;

    s->last_index = b->index;
    return 0;

error:
    set_error(s, ENOMEM);
    return -1;
}

int stream_init(struct stream *s, int id, const struct config *config)
{
    int err;

    s->config = config;
    s->root_ctx = NULL;
    s->block_ctx = NULL;
    s->last_index = id - (int)config->streams;
    s->id = id;
    s->error = 0;

    s->root_ctx = EVP_MD_CTX_new();
    if (s->root_ctx == NULL)
        return ENOMEM;

    if (!EVP_DigestInit_ex(s->root_ctx, config->md, NULL)) {
        err = ENOMEM;
        goto error;
    }

    s->block_ctx = EVP_MD_CTX_new();
    if (s->block_ctx == NULL) {
        err = ENOMEM;
        goto  error;
    }

    return 0;

error:
    stream_destroy(s);
    return err;
}

int stream_update(struct stream *s, struct block *b)
{
    if (s->error)
        return s->error;

    add_zero_blocks_before(s, b);

    if (b->len)
        add_data_block(s, b);

    return s->error;
}

int stream_final(struct stream *s, unsigned char *md, unsigned int *len)
{
    if (s->error)
        return s->error;

    if (!EVP_DigestFinal_ex(s->root_ctx, md, len))
        return ENOMEM;

    return 0;
}

void stream_destroy(struct stream *s)
{
    EVP_MD_CTX_free(s->block_ctx);
    s->block_ctx = NULL;

    EVP_MD_CTX_free(s->root_ctx);
    s->root_ctx = NULL;
}