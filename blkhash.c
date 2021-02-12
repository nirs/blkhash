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

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "blkhash.h"

struct blkhash {
    size_t block_size;
    const EVP_MD *md;

    /* For computing root digest. */
    EVP_MD_CTX *root_ctx;

    /* For computing block digests. */
    unsigned char *block;
    EVP_MD_CTX *block_ctx;

    /* Precomputed zero block digest. */
    unsigned char zero_md[EVP_MAX_MD_SIZE];
    unsigned int zero_md_len;
};

/*
 * Based on Rusty Russell's memeqzero.
 * See http://rusty.ozlabs.org/?p=560 for more info.
 */
static bool is_zero(void *buf, size_t buf_len)
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

static void blkhash_block_digest(struct blkhash *h, unsigned int len,
                                 unsigned char *md_value, unsigned int *md_len)
{
    EVP_DigestInit_ex(h->block_ctx, h->md, NULL);
    EVP_DigestUpdate(h->block_ctx, h->block, len);
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

    h->block = calloc(1, block_size);
    if (h->block == NULL) {
        goto error;
    }

    EVP_DigestInit_ex(h->root_ctx, h->md, NULL);

    /* Compute the zero block digest. This digest will be used for zero
     * blocks later. */
    blkhash_block_digest(h, block_size, h->zero_md, &h->zero_md_len);

    return h;

error:
    saved_errno = errno;
    blkhash_free(h);
    errno = saved_errno;

    return NULL;
}

int blkhash_read(struct blkhash *h, int fd)
{
    size_t pos = 0;

    while (pos < h->block_size) {
        ssize_t n;

        do {
            n = read(fd, h->block + pos, h->block_size - pos);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            break;
        }
        pos += n;
    }

    if (pos == h->block_size && is_zero(h->block, pos)) {
        /* Fast path - reuse precomputed zero md. Detecting zero block
         * is order of magnitude faster than computing md. */
        EVP_DigestUpdate(h->root_ctx, h->zero_md, h->zero_md_len);
    } else if (pos > 0) {
        /* Slow path - compute md for this block. */
        unsigned char md_value[EVP_MAX_MD_SIZE];
        unsigned int md_len;

        blkhash_block_digest(h, pos, md_value, &md_len);
        EVP_DigestUpdate(h->root_ctx, md_value, md_len);
    }

    return pos;
}

void blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len)
{
    EVP_DigestFinal_ex(h->root_ctx, md_value, md_len);
}

void blkhash_reset(struct blkhash *h)
{
    EVP_DigestInit_ex(h->root_ctx, h->md, NULL);
}

void blkhash_free(struct blkhash *h)
{
    if (h == NULL)
        return;

    EVP_MD_CTX_free(h->root_ctx);
    EVP_MD_CTX_free(h->block_ctx);
    free(h->block);

    free(h);
}
