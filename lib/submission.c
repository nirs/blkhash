// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdlib.h>
#include <string.h>

#include "blkhash-internal.h"

struct submission *submission_new(enum submission_type type, struct stream *stream,
                             int64_t index, size_t len, const void *data)
{
    struct submission *sub = malloc(sizeof(*sub) + len);
    if (sub == NULL)
        return NULL;

    sub->type = type;
    sub->stream = stream;
    sub->index = index;
    sub->len = len;

    if (len)
        memcpy(sub->data, data, len);

    return sub;
}

void submission_free(struct submission *sub)
{
    free(sub);
}
