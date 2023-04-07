// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdlib.h>
#include <string.h>

#include "blkhash-internal.h"

struct submission *submission_new_data(struct stream *stream, int64_t index,
                                       size_t len, const void *data)
{
    struct submission *sub;

    sub = malloc(sizeof(*sub) + len);
    if (sub == NULL)
        return NULL;

    sub->type = DATA;
    sub->stream = stream;
    sub->index = index;
    sub->len = len;

    memcpy(sub->data, data, len);

    return sub;
}

struct submission *submission_new_zero(struct stream *stream, int64_t index)
{
    struct submission *sub;

    sub = malloc(sizeof(*sub));
    if (sub == NULL)
        return NULL;

    sub->type = ZERO;
    sub->stream = stream;
    sub->index = index;

    return sub;
}

struct submission *submission_new_stop(void)
{
    struct submission *sub;

    sub = malloc(sizeof(*sub));
    if (sub == NULL)
        return NULL;

    sub->type = STOP;

    return sub;
}

void submission_free(struct submission *sub)
{
    free(sub);
}
