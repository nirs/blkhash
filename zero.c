// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <string.h>

#include "blkhash_internal.h"

/*
 * Based on Rusty Russell's memeqzero.
 * See http://rusty.ozlabs.org/?p=560 for more info.
 */
bool is_zero(const void *buf, size_t len)
{
    const unsigned char *p;
    size_t i;

    p = buf;

    /* Check first 16 bytes manually. */
    for (i = 0; i < 16; i++) {
        if (len == 0) {
            return true;
        }

        if (*p) {
            return false;
        }

        p++;
        len--;
    }

    /* Now we know that's zero, memcmp with self. */
    return memcmp(buf, p, len) == 0;
}
