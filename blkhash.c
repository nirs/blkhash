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

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define WORKERS 4

struct blkhash {
    struct config config;
    struct worker workers[WORKERS];

    /* For computing root digest. */
    EVP_MD_CTX *root_ctx;

    /* For keeping partial blocks when user call blkhash_update() with buffer
     * that is not aligned to block size. */
    unsigned char *pending;
    size_t pending_len;
    bool pending_zero;

    /* Current block index, increased when consuming a data or zero block. */
    int64_t block_index;

    /* Image size, increased when adding data or zero to the hash. */
    int64_t image_size;

    /* Set when hash is finalized. */
    bool finalized;
};

struct blkhash *blkhash_new(size_t block_size, const char *md_name)
{
    struct blkhash *h;
    int err;

    h = calloc(1, sizeof(*h));
    if (h == NULL)
        return NULL;

    err = config_init(&h->config, md_name, block_size, WORKERS);
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

    for (int i = 0; i < WORKERS; i++)
        worker_init(&h->workers[i], i, &h->config);

    return h;

error:
    blkhash_free(h);
    errno = err;

    return NULL;
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

static inline void skip_zero_block(struct blkhash *h)
{
    h->block_index++;
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
static void consume_data(struct blkhash *h, const void *buf, size_t len)
{
    if (is_zero_block(h, buf, len)) {
        /* Fast path. */
        skip_zero_block(h);
    } else {
        /* Slow path. */
        struct block *block = block_new(h->block_index, len, buf);
        struct worker *worker = &h->workers[h->block_index % WORKERS];
        worker_update(worker, block);
        h->block_index++;
    }
}

/*
 * Consume all pending data or zeros. The pending buffer may contain
 * full or partial block of data or zeros. The pending buffer is
 * cleared after this call.
 */
static void consume_pending(struct blkhash *h)
{
    assert(h->pending_len <= h->config.block_size);

    if (h->pending_len == h->config.block_size && h->pending_zero) {
        /* Fast path. */
        skip_zero_block(h);
    } else {
        /*
         * Slow path if pending is partial block, fast path is pending
         * is full block and pending data is zeros.
         */
        if (h->pending_zero) {
            /* Convert partial block of zeros to data. */
            memset(h->pending, 0, h->pending_len);
        }

        consume_data(h, h->pending, h->pending_len);
    }

    h->pending_len = 0;
    h->pending_zero = false;
}

void blkhash_update(struct blkhash *h, const void *buf, size_t len)
{
    h->image_size += len;

    /* Try to fill the pending buffer and consume it. */
    if (h->pending_len > 0) {
        size_t n = add_pending_data(h, buf, len);
        buf += n;
        len -= n;
        if (h->pending_len == h->config.block_size) {
            consume_pending(h);
        }
    }

    /* Consume all full blocks in caller buffer. */
    while (len >= h->config.block_size) {
        consume_data(h, buf, h->config.block_size);
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
}

void blkhash_zero(struct blkhash *h, size_t len)
{
    h->image_size += len;

    /* Try to fill the pending buffer and consume it. */
    if (h->pending_len > 0) {
        len -= add_pending_zeros(h, len);
        if (h->pending_len == h->config.block_size) {
            consume_pending(h);
        }
    }

    /* Consume all full zero blocks. */
    while (len >= h->config.block_size) {
        skip_zero_block(h);
        len -= h->config.block_size;
    }

    /* Save the rest in the pending buffer. */
    if (len > 0) {
        len -= add_pending_zeros(h, len);
    }

    assert(len == 0);
}

int blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len)
{
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len;
    int err = 0;

    if (h->finalized)
        return EINVAL;

    h->finalized = true;

    if (h->pending_len > 0) {
        consume_pending(h);
    }

    for (int i = 0; i < WORKERS; i++)
        worker_final(&h->workers[i], h->image_size);

    for (int i = 0; i < WORKERS; i++) {
        worker_digest(&h->workers[i], md, &len);
        if (!EVP_DigestUpdate(h->root_ctx, md, len)) {
            if (err == 0)
                err = ENOMEM;
        }
    }

    if (err)
        return err;

    if (!EVP_DigestFinal_ex(h->root_ctx, md_value, md_len))
        return ENOMEM;

    return 0;
}

void blkhash_free(struct blkhash *h)
{
    if (h == NULL)
        return;

    if (!h->finalized) {
        unsigned char drop[EVP_MAX_MD_SIZE];
        blkhash_final(h, drop, NULL);
    }

    for (int i = 0; i < WORKERS; i++)
        worker_destroy(&h->workers[i]);

    EVP_MD_CTX_free(h->root_ctx);
    free(h->pending);

    free(h);
}
