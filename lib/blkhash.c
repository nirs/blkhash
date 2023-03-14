// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "blkhash.h"
#include "blkhash_internal.h"
#include "util.h"

/* Number of consecutive zero blocks to consume before submitting zero length
 * block to all workers. */
#define ZERO_BLOCKS_BATCH_SIZE (64 * 1024)

struct blkhash {
    struct config config;

    /* For computing root digest. */
    EVP_MD_CTX *root_ctx;

    /* For keeping partial blocks when user call blkhash_update() with buffer
     * that is not aligned to block size. */
    unsigned char *pending;
    size_t pending_len;
    bool pending_zero;

    /* Current block index, increased when consuming a data or zero block. */
    int64_t block_index;

    /* The index of the last submitted block. */
    int64_t update_index;

    /* Image size, increased when adding data or zero to the hash. */
    int64_t image_size;

    /* The first error. Once set, any operation will fail quickly with this
     * error. */
    int error;

    /* Set when hash is finalized. */
    bool finalized;

    unsigned workers_count;
    struct worker workers[0];
};

/* Set the error and return -1. All intenral errors should be handled with
 * this. Public APIs should always return the internal error on failures. */
static inline int set_error(struct blkhash *h, int error)
{
    if (h->error == 0)
        h->error = error;

    return -1;
}

struct blkhash *blkhash_new(const char *md_name, size_t block_size, unsigned threads)
{
    struct blkhash *h;
    int err;

    h = calloc(1, sizeof(*h) + sizeof(h->workers[0]) * threads);
    if (h == NULL)
        return NULL;

    err = config_init(&h->config, md_name, block_size, threads);
    if (err)
        goto error;

    h->root_ctx = EVP_MD_CTX_new();
    if (h->root_ctx == NULL) {
        err = ENOMEM;
        goto error;
    }

    h->pending = calloc(1, block_size);
    if (h->pending == NULL) {
        err = errno;
        goto error;
    }

    h->pending_len = 0;

    if (!EVP_DigestInit_ex(h->root_ctx, h->config.md, NULL)) {
        err = ENOMEM;
        goto error;
    }

    for (unsigned i = 0; i < threads; i++) {
        err = worker_init(&h->workers[i], i, &h->config);
        if (err)
            goto error;

        h->workers_count++;
    }

    return h;

error:
    blkhash_free(h);
    errno = err;

    return NULL;
}

/*
 * Submit one zero length block to all workers. Every worker will add zero
 * blocks from the last data block to the submitted block index.
 */
static int submit_zero_block(struct blkhash *h)
{
    struct block *b;
    int err;

    for (int i = 0; i < h->config.workers; i++) {
        b = block_new(h->block_index, 0, NULL);
        if (b == NULL)
            return set_error(h, errno);

        err = worker_update(&h->workers[i], b);
        if (err) {
            block_free(b);
            return set_error(h, err);
        }
    }

    h->update_index = h->block_index;
    return 0;
}

/*
 * Submit one data block to the worker handling this block.
 */
static int submit_data_block(struct blkhash *h, const void *buf, size_t len)
{
    struct block *b;
    struct worker *w;
    int err;

    b = block_new(h->block_index, len, buf);
    if (b == NULL)
        return set_error(h, errno);

    w = &h->workers[h->block_index % h->config.workers];
    err = worker_update(w, b);
    if (err) {
        block_free(b);
        return set_error(h, err);
    }

    h->update_index = h->block_index;
    h->block_index++;
    return 0;
}

/*
 * Add up to block_size bytes of data to the pending buffer, trying to
 * fill the pending buffer. If the buffer kept pending zeros, convert
 * the pending zeros to data.
 *
 * Return the number of bytes added to the pending buffer.
 */
static size_t add_pending_data(struct blkhash *h, const void *buf, size_t len)
{
    size_t n = MIN(len, h->config.block_size - h->pending_len);

    if (h->pending_zero) {
        /*
         * The buffer contains zeros, convert pending zeros to data.
         */
        memset(h->pending, 0, h->pending_len);
        h->pending_zero = false;
    }

    memcpy(h->pending + h->pending_len, buf, n);

    h->pending_len += n;

    return n;
}

/*
 * Add up to block_size zero bytes to the pending buffer, trying to fill
 * the pending buffer. If the buffer kept pending data, convert the
 * zeros to data.
 *
 * Return the number of bytes added to the pending buffer.
 */
static size_t add_pending_zeros(struct blkhash *h, size_t len)
{
    size_t n = MIN(len, h->config.block_size - h->pending_len);

    if (h->pending_len == 0) {
        /* The buffer is empty, start collecting pending zeros. */
        h->pending_zero = true;
    } else if (!h->pending_zero) {
        /*
         * The buffer contains some data, convert the zeros to pending data.
         */
        memset(h->pending + h->pending_len, 0, n);
    }

    h->pending_len += n;

    return n;
}

/*
 * Consume count zero blocks, sending zero length block to all workers if
 * enough zero blocks were consumed since the last data block.
 */
static inline int consume_zero_blocks(struct blkhash *h, size_t count)
{
    h->block_index += count;

    if (h->block_index - h->update_index >= ZERO_BLOCKS_BATCH_SIZE)
        return submit_zero_block(h);

    return 0;
}

static inline bool is_zero_block(struct blkhash *h, const void *buf, size_t len)
{
    return len == h->config.block_size && is_zero(buf, len);
}

/*
 * Consume len bytes of data from buf. If called with a full block, try
 * to speed the computation by detecting zeros. Detecting zeros in
 * order of magnitude faster compared with computing a message digest.
 */
static int consume_data_block(struct blkhash *h, const void *buf, size_t len)
{
    if (is_zero_block(h, buf, len)) {
        /* Fast path. */
        return consume_zero_blocks(h, 1);
    } else {
        /* Slow path. */
        return submit_data_block(h, buf, len);
    }
}

/*
 * Consume all pending data or zeros. The pending buffer may contain
 * full or partial block of data or zeros. The pending buffer is
 * cleared after this call.
 */
static int consume_pending(struct blkhash *h)
{
    assert(h->pending_len <= h->config.block_size);

    if (h->pending_len == h->config.block_size && h->pending_zero) {
        /* Fast path. */
        if (consume_zero_blocks(h, 1))
            return -1;
    } else {
        /*
         * Slow path if pending is partial block, fast path is pending
         * is full block and pending data is zeros.
         */
        if (h->pending_zero) {
            /* Convert partial block of zeros to data. */
            memset(h->pending, 0, h->pending_len);
        }

        if (consume_data_block(h, h->pending, h->pending_len))
            return -1;
    }

    h->pending_len = 0;
    h->pending_zero = false;
    return 0;
}

int blkhash_update(struct blkhash *h, const void *buf, size_t len)
{
    if (h->error)
        return h->error;

    h->image_size += len;

    /* Try to fill the pending buffer and consume it. */
    if (h->pending_len > 0) {
        size_t n = add_pending_data(h, buf, len);
        buf += n;
        len -= n;
        if (h->pending_len == h->config.block_size) {
            if (consume_pending(h))
                return h->error;
        }
    }

    /* Consume all full blocks in caller buffer. */
    while (len >= h->config.block_size) {
        if (consume_data_block(h, buf, h->config.block_size))
            return h->error;

        buf += h->config.block_size;
        len -= h->config.block_size;
    }

    /* Copy rest of the data to the internal buffer. */
    if (len > 0) {
        size_t n = add_pending_data(h, buf, len);
        buf += n;
        len -= n;
    }

    assert(len == 0);
    return 0;
}

int blkhash_zero(struct blkhash *h, size_t len)
{
    if (h->error)
        return h->error;

    h->image_size += len;

    /* Try to fill the pending buffer and consume it. */
    if (h->pending_len > 0) {
        len -= add_pending_zeros(h, len);
        if (h->pending_len == h->config.block_size) {
            if (consume_pending(h))
                return h->error;
        }
    }

    /* Consume all full zero blocks. */
    if (len >= h->config.block_size) {
        if (consume_zero_blocks(h, len / h->config.block_size))
            return h->error;

        len %= h->config.block_size;
    }

    /* Save the rest in the pending buffer. */
    if (len > 0) {
        len -= add_pending_zeros(h, len);
    }

    assert(len == 0);
    return 0;
}

static void stop_workers(struct blkhash *h, bool want_digest)
{
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len;
    int err;

    for (unsigned i = 0; i < h->workers_count; i++) {
        err = worker_final(&h->workers[i], want_digest ? h->image_size : 0);
        if (err)
            set_error(h, err);
    }

    for (unsigned i = 0; i < h->workers_count; i++) {
        err = worker_digest(&h->workers[i], md, &len);
        if (err)
            set_error(h, err);

        if (want_digest && h->error == 0) {
            if (!EVP_DigestUpdate(h->root_ctx, md, len))
                set_error(h, ENOMEM);
        }
    }
}

int blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len)
{
    bool want_digest = true;

    if (h->finalized)
        return EINVAL;

    h->finalized = true;

    if (h->pending_len > 0)
        consume_pending(h);

    want_digest = h->error == 0;
    stop_workers(h, want_digest);

    if (h->error == 0) {
        if (!EVP_DigestFinal_ex(h->root_ctx, md_value, md_len))
            set_error(h, ENOMEM);
    }

    return h->error;
}

void blkhash_free(struct blkhash *h)
{
    if (h == NULL)
        return;

    if (!h->finalized) {
        bool want_digest = false;
        stop_workers(h, want_digest);
    }

    for (unsigned i = 0; i < h->workers_count; i++)
        worker_destroy(&h->workers[i]);

    EVP_MD_CTX_free(h->root_ctx);
    free(h->pending);

    free(h);
}
