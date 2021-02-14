/*
 * blkhash - block based hash
 * Copyright Nir Soffer <nirsof@gmail.com>.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

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

    /* Precomputed zero block digest. */
    unsigned char zero_md[EVP_MAX_MD_SIZE];
    unsigned int zero_md_len;
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

static size_t fill_pending(struct blkhash *h, const void *buf, size_t len)
{
    size_t n = MIN(len, h->block_size - h->pending_len);

    memcpy(h->pending + h->pending_len, buf, n);

    h->pending_len += n;

    return n;
}

static void consume_block(struct blkhash *h, const void *buf, size_t len)
{
    if (len == h->block_size && is_zero(buf, len)) {
        /* Fast path - reuse precomputed zero md. Detecting zero block
         * is order of magnitude faster than computing md. */
        EVP_DigestUpdate(h->root_ctx, h->zero_md, h->zero_md_len);
    } else {
        /* Slow path - compute md for this block. */
        unsigned char md_value[EVP_MAX_MD_SIZE];
        unsigned int md_len;

        compute_digest(h, buf, len, md_value, &md_len);
        EVP_DigestUpdate(h->root_ctx, md_value, md_len);
    }
}

void blkhash_update(struct blkhash *h, const void *buf, size_t len)
{
    /*
     * If we have pending data try to fill the internal buffer and consume the
     * data.
     */
    if (h->pending_len > 0) {
        size_t n = fill_pending(h, buf, len);
        buf += n;
        len -= n;

        if (h->pending_len == h->block_size) {
            consume_block(h, h->pending, h->pending_len);
            h->pending_len = 0;
        }
    }

    /*
     * Consume full blocks in caller buffer.
     */
    while (len >= h->block_size) {
        consume_block(h, buf, h->block_size);
        buf += h->block_size;
        len -= h->block_size;
    }

    /*
     * If caller buffer was not aligned to block size, copy the data to the
     * internal buffer.
     */
    if (len > 0) {
        size_t n = fill_pending(h, buf, len);
        buf += n;
        len -= n;
    }

    assert(len == 0);
}

void blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len)
{
    if (h->pending_len > 0) {
        consume_block(h, h->pending, h->pending_len);
        h->pending_len = 0;
    }

    EVP_DigestFinal_ex(h->root_ctx, md_value, md_len);
}

void blkhash_reset(struct blkhash *h)
{
    h->pending_len = 0;

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
