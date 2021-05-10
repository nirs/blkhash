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

#include <unistd.h>
#include <openssl/evp.h>

#include "blkhash.h"
#include "blksum.h"
#include "util.h"

void simple_checksum(struct src *s, struct options *opt, unsigned char *out)
{
    struct blkhash *h;
    void *buf;
    const EVP_MD *md;
    int md_size;
    EVP_MD_CTX *md_ctx;
    unsigned char seg_md[EVP_MAX_MD_SIZE];

    md = EVP_get_digestbyname(opt->digest_name);
    if (md == NULL)
        FAIL_ERRNO("EVP_get_digestbyname");

    md_size = EVP_MD_size(md);

    h = blkhash_new(opt->block_size, opt->digest_name);
    if (h == NULL)
        FAIL_ERRNO("blkhash_new");

    buf = malloc(opt->read_size);
    if (buf == NULL)
        FAIL_ERRNO("malloc");

    md_ctx = EVP_MD_CTX_new();
    if (md_ctx == NULL)
        FAIL_ERRNO("EVP_MD_CTX_new");

    EVP_DigestInit_ex(md_ctx, md, NULL);

    for (size_t i = 0;; i++) {
        size_t pos = 0;

        while (pos < opt->segment_size) {
            size_t count = src_read(s, buf, opt->read_size);
            if (count == 0)
                break;

            blkhash_update(h, buf, count);
            pos += count;
        }

        if (pos == 0)
            break;

        blkhash_final(h, seg_md, NULL);
        blkhash_reset(h);

        if (debug) {
            char hex[EVP_MAX_MD_SIZE * 2 + 1];

            format_hex(seg_md, md_size, hex);
            DEBUG("segment %ld offset %ld checksum %s",
                  i, i * opt->segment_size, hex);
        }

        EVP_DigestUpdate(md_ctx, seg_md, md_size);
    }

    EVP_DigestFinal_ex(md_ctx, out, NULL);

    EVP_MD_CTX_free(md_ctx);
    free(buf);
    blkhash_free(h);
}
