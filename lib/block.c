// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdlib.h>
#include <string.h>

#include "blkhash_internal.h"

struct block *block_new(uint64_t index, size_t len, const void *data)
{
    struct block *b = malloc(sizeof(*b) + len);
    if (b == NULL)
        return NULL;

    b->index = index;
    b->len = len;
    b->last = false;

    if (len)
        memcpy(b->data, data, len);

    return b;
}

void block_free(struct block *b)
{
    free(b);
}
