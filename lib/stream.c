// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdlib.h>

#include "blkhash-internal.h"
#include "digest.h"
#include "util.h"

static inline void set_error(struct stream *s, int error)
{
    /* Keep the first error. */
    if (s->error == 0)
        s->error = error;
}

static int add_zero_blocks_before(struct stream *s, const struct submission *sub)
{
    int64_t index;
    int err;

    /* Don't modify the hash after errors. */
    if (s->error)
        return -1;

    index = s->last_index + s->config->streams;

    while (index < sub->index) {
        err = -digest_update(s->root_digest, s->config->zero_md, s->config->md_len);
        if (err) {
            set_error(s, err);
            return -1;
        }
        s->last_index = index;
        index += s->config->streams;
    }

    return 0;
}

static inline bool is_zero_block(struct stream *s, const struct submission *sub)
{
    return !(sub->flags & SUBMIT_COPY_DATA) &&
        sub->len == s->config->block_size &&
        is_zero(sub->data, sub->len);
}

static int compute_block_digest(struct stream *s, const struct submission *sub,
                                unsigned char *md, unsigned int *len)
{
    int err;

    err = -digest_init(s->block_digest);
    if (err)
        return err;

    err = -digest_update(s->block_digest, sub->data, sub->len);
    if (err)
        return err;

    return -digest_final(s->block_digest, md, len);
}

static int add_data_block(struct stream *s, const struct submission *sub)
{
    int err;

    /* Don't modify the hash after errors. */
    if (s->error)
        return -1;

    if (is_zero_block(s, sub)) {
        /* Fast path */
        err = -digest_update(s->root_digest, s->config->zero_md, s->config->md_len);
        if (err)
            goto error;
    } else {
        /* Slow path */
        unsigned char block_md[BLKHASH_MAX_MD_SIZE];
        unsigned int len;

        err = compute_block_digest(s, sub, block_md, &len);
        if (err)
            goto error;

        err = -digest_update(s->root_digest, block_md, len);
        if (err)
            goto error;
    }

    s->last_index = sub->index;
    return 0;

error:
    set_error(s, err);
    return -1;
}

int stream_init(struct stream *s, int id, const struct config *config)
{
    int err;

    s->config = config;
    s->root_digest = NULL;
    s->block_digest = NULL;
    s->last_index = id - (int)config->streams;
    s->id = id;
    s->error = 0;

    err = -digest_create(config->digest_name, &s->root_digest);
    if (err)
        return err;

    err = -digest_init(s->root_digest);
    if (err)
        goto error;

    err = -digest_create(config->digest_name, &s->block_digest);
    if (err)
        goto error;

    return 0;

error:
    stream_destroy(s);
    return err;
}

int stream_update(struct stream *s, struct submission *sub)
{
    if (s->error)
        return s->error;

    add_zero_blocks_before(s, sub);

    if (sub->type == DATA)
        add_data_block(s, sub);

    if (s->error) {
        submission_set_error(sub, s->error);
        return s->error;
    }

    return 0;
}

int stream_final(struct stream *s, unsigned char *md, unsigned int *len)
{
    int err;

    if (s->error)
        return s->error;

    err = -digest_final(s->root_digest, md, len);
    if (err) {
        set_error(s, err);
        return err;
    }

    return 0;
}

void stream_destroy(struct stream *s)
{
    digest_destroy(s->block_digest);
    s->block_digest = NULL;

    digest_destroy(s->root_digest);
    s->root_digest = NULL;
}
