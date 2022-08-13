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

#define MIN(a,b) ((a) < (b) ? (a) : (b))

struct blkhash {
    size_t block_size;
    const EVP_MD *md;

    /* For computing root digest. */
    EVP_MD_CTX *root_ctx;

    /* For computing block digests. */
    EVP_MD_CTX *block_ctx;

    /* For keeping partial blocks when user call blkhash_update() with buffer
     * that is not aligned to block size. */
    unsigned char *pending;
    size_t pending_len;
    bool pending_zero;

    /* Precomputed zero block digest. */
    unsigned char zero_md[EVP_MAX_MD_SIZE];
    unsigned int zero_md_len;

    /* Current block index, increased when consuming a data or zero block. */
    int64_t block_index;

    /* Image size, increased when adding data or zero to the hash. */
    int64_t image_size;
};

/*
 * Based on Rusty Russell's memeqzero.
 * See http://rusty.ozlabs.org/?p=560 for more info.
 */
static bool is_zero(const void *buf, size_t buf_len)
{
    const unsigned char *p;
    size_t i;

    p = buf;

    /* Check first 16 bytes manually. */
    for (i = 0; i < 16; i++) {
        if (buf_len == 0) {
            return true;
        }

        if (*p) {
            return false;
        }

        p++;
        buf_len--;
    }

    /* Now we know that's zero, memcmp with self. */
    return memcmp(buf, p, buf_len) == 0;
}

static void compute_digest(struct blkhash *h, const void *buf, unsigned int buf_len,
                           unsigned char *md_value, unsigned int *md_len)
{
    EVP_DigestInit_ex(h->block_ctx, h->md, NULL);
    EVP_DigestUpdate(h->block_ctx, buf, buf_len);
    EVP_DigestFinal_ex(h->block_ctx, md_value, md_len);
}

struct blkhash *blkhash_new(size_t block_size, const char *md_name)
{
    struct blkhash *h;
    int saved_errno;

    h = calloc(1, sizeof(*h));
    if (h == NULL) {
        return NULL;
    }

    h->block_size = block_size;

    h->md = EVP_get_digestbyname(md_name);
    if (h->md == NULL) {
        goto error;
    }

    h->root_ctx = EVP_MD_CTX_new();
    if (h->root_ctx == NULL) {
        goto error;
    }

    h->block_ctx = EVP_MD_CTX_new();
    if (h->block_ctx == NULL) {
        goto error;
    }

    h->pending = calloc(1, block_size);
    if (h->pending == NULL) {
        goto error;
    }

    h->pending_len = 0;

    EVP_DigestInit_ex(h->root_ctx, h->md, NULL);

    /* Compute the zero block digest. This digest will be used for zero
     * blocks later. */
    compute_digest(h, h->pending, block_size, h->zero_md, &h->zero_md_len);

    return h;

error:
    saved_errno = errno;
    blkhash_free(h);
    errno = saved_errno;

    return NULL;
}

/*
 * Add up to block_size bytes of data to the pending buffer, trying to
 * fill the pending buffer. If the buffer kept pending zeroes, convert
 * the pending zeroes to data.
 *
 * Return the number of bytes added to the pending buffer.
 */
static size_t add_pending_data(struct blkhash *h, const void *buf, size_t len)
{
    size_t n = MIN(len, h->block_size - h->pending_len);

    if (h->pending_zero) {
        /*
         * The buffer contains zeroes, convert pending zeros to data.
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
 * zeroes to data.
 *
 * Return the number of bytes added to the pending buffer.
 */
static size_t add_pending_zeroes(struct blkhash *h, size_t len)
{
    size_t n = MIN(len, h->block_size - h->pending_len);

    if (h->pending_len == 0) {
        /* The buffer is empty, start collecting pending zeroes. */
        h->pending_zero = true;
    } else if (!h->pending_zero) {
        /*
         * The buffer contains some data, convert the zeroes to pending data.
         */
        memset(h->pending + h->pending_len, 0, n);
    }

    h->pending_len += n;

    return n;
}

/*
 * Consume full block of zeroes. This is several orders of magnitude
 * faster than computing message digest.
 */
static inline void consume_zero_block(struct blkhash *h)
{
    EVP_DigestUpdate(h->root_ctx, h->zero_md, h->zero_md_len);
    h->block_index++;
}

/*
 * Consume len bytes of data from buf. If called with a full block, try
 * to speed the computation by detecting zeroes. Detecting zeroes in
 * order of magnitude faster compared with computing a message digest.
 */
static void consume_data(struct blkhash *h, const void *buf, size_t len)
{
    if (len == h->block_size && is_zero(buf, len)) {
        /* Fast path. */
        consume_zero_block(h);
    } else {
        /* Slow path - compute md for buf. */
        unsigned char md_value[EVP_MAX_MD_SIZE];
        unsigned int md_len;

        compute_digest(h, buf, len, md_value, &md_len);
        EVP_DigestUpdate(h->root_ctx, md_value, md_len);
    }
    h->block_index++;
}

/*
 * Consume all pending data or zeroes. The pending buffer may contain
 * full or partial block of data or zeroes. The pending buffer is
 * cleared after this call.
 */
static void consume_pending(struct blkhash *h)
{
    assert(h->pending_len <= h->block_size);

    if (h->pending_len == h->block_size && h->pending_zero) {
        /* Very fast path. */
        consume_zero_block(h);
    } else {
        /*
         * Slow path if pending is partial block, fast path is pending
         * is full block and pending data is zeroes.
         */
        if (h->pending_zero) {
            /* Convert partial block of zeroes to data. */
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
        if (h->pending_len == h->block_size) {
            consume_pending(h);
        }
    }

    /* Consume all full blocks in caller buffer. */
    while (len >= h->block_size) {
        consume_data(h, buf, h->block_size);
        buf += h->block_size;
        len -= h->block_size;
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
        len -= add_pending_zeroes(h, len);
        if (h->pending_len == h->block_size) {
            consume_pending(h);
        }
    }

    /* Consume all full zero blocks. */
    while (len >= h->block_size) {
        consume_zero_block(h);
        len -= h->block_size;
    }

    /* Save the rest in the pending buffer. */
    if (len > 0) {
        len -= add_pending_zeroes(h, len);
    }

    assert(len == 0);
}

void blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len)
{
    if (h->pending_len > 0) {
        consume_pending(h);
    }

    EVP_DigestFinal_ex(h->root_ctx, md_value, md_len);
}

void blkhash_reset(struct blkhash *h)
{
    h->pending_len = 0;
    h->block_index = 0;

    EVP_DigestInit_ex(h->root_ctx, h->md, NULL);
}

void blkhash_free(struct blkhash *h)
{
    if (h == NULL)
        return;

    EVP_MD_CTX_free(h->root_ctx);
    EVP_MD_CTX_free(h->block_ctx);
    free(h->pending);

    free(h);
}
