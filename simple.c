// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdlib.h>
#include <unistd.h>

#include "blkhash.h"
#include "blksum.h"
#include "util.h"

void simple_checksum(struct src *s, struct options *opt, unsigned char *out)
{
    void *buf;
    struct blkhash *h;

    buf = malloc(opt->read_size);
    if (buf == NULL)
        FAIL_ERRNO("malloc");

    h = blkhash_new(opt->block_size, opt->digest_name);
    if (h == NULL)
        FAIL_ERRNO("blkhash_new");

    while (running()) {
        size_t count = src_read(s, buf, opt->read_size);
        if (count == 0)
            break;

        if (!io_only)
            blkhash_update(h, buf, count);
    }

    blkhash_final(h, out, NULL);

    blkhash_free(h);
    free(buf);
}
