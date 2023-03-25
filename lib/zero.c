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
    uint64_t n[2];

    /* We always use full blocks. */
    assert(len >= 16);

    /* Ensure aligned memory, may be required by some architectures:
     * https://www.kernel.org/doc/html/next/core-api/unaligned-memory-access.html */
    memcpy(n, buf, 16);

    /* Check first 16 bytes manually. */
    if (n[0] | n[1])
        return false;

    /* Now we know that's zero, memcmp with self. */
    return memcmp(buf, buf + 16, len - 16) == 0;
}
