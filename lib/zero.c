// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <string.h>

#include "blkhash-internal.h"

/*
 * Based on Rusty Russell's memeqzero.
 * See http://rusty.ozlabs.org/?p=560 for more info.
 */
bool is_zero(const void *buf, size_t len)
{
    const unsigned char *p;
    const unsigned char *c;

    /* We always use full blocks. */
    assert(len >= 16);

    p = buf;

    /* Check first 16 bytes manually. */
    for (c = p; c < p + 16; c += 8) {
        if (c[0] | c[1] | c[2] | c[4] | c[5] | c[6] | c[7])
            return false;
    }

    /* Now we know that's zero, memcmp with self. */
    return memcmp(buf, p + 16, len - 16) == 0;
}
