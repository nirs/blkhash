// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdlib.h>
#include <unistd.h>

#include "blkhash.h"
#include "blksum.h"
#include "util.h"
#include "src.h"

void checksum(struct src *s, struct options *opt, unsigned char *out,
              unsigned int *len)
{
    void *buf;
    struct blkhash *h;
    struct blkhash_opts *ho;
    int err = 0;

    buf = malloc(opt->read_size);
    if (buf == NULL)
        FAIL_ERRNO("malloc");

    ho = blkhash_opts_new(opt->digest_name);
    if (ho == NULL)
        FAIL_ERRNO("blkhash_opts_new");

    if (blkhash_opts_set_block_size(ho, opt->block_size))
        FAIL("Invalid block size value: %zu", opt->block_size);

    if (blkhash_opts_set_streams(ho, opt->streams))
        FAIL("Invalid streams value: %zu", opt->streams);

    if (blkhash_opts_set_threads(ho, opt->threads))
        FAIL("Invalid threads value: %zu", opt->threads);

    h = blkhash_new_opts(ho);
    blkhash_opts_free(ho);
    if (h == NULL)
        FAIL_ERRNO("blkhash_new");

    while (running()) {
        size_t count = src_read(s, buf, opt->read_size);
        if (count == 0)
            break;

        err = blkhash_update(h, buf, count);
        if (err) {
            ERROR("blkhash_update: %s", strerror(err));
            goto out;
        }
    }

    if (running()) {
        err = blkhash_final(h, out, len);
        if (err)
            ERROR("blkhash_final: %s", strerror(err));
    }

out:
    blkhash_free(h);
    free(buf);

    if (err)
        exit(EXIT_FAILURE);
}
