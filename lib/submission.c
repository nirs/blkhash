// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "blkhash-internal.h"

static int copy_data(struct submission *sub)
{
    void *data;

    data = malloc(sub->len);
    if (data == NULL)
        return errno;

    memcpy(data, sub->data, sub->len);
    sub->data = data;

    return 0;
}

int submission_init_data(struct submission *sub, struct stream *stream,
                         int64_t index, size_t len, const void *data,
                         struct completion *completion, uint8_t flags)
{
    sub->type = DATA;
    sub->stream = stream;
    sub->completion = completion;
    sub->data = data;
    sub->index = index;
    sub->len = len;
    sub->flags = flags;

    if (flags & SUBMIT_COPY_DATA) {
        int err = copy_data(sub);
        if (err)
            return err;
    }

    if (sub->completion)
        completion_ref(sub->completion);

    return 0;
}

void submission_init_zero(struct submission *sub, struct stream *stream, int64_t index)
{
    memset(sub, 0, sizeof(*sub));

    sub->type = ZERO;
    sub->stream = stream;
    sub->index = index;
}

void submission_set_error(struct submission *sub, int error)
{
    if (sub->completion)
        completion_set_error(sub->completion, error);
}

void submission_destroy(struct submission *sub)
{
    if (sub->type == DATA) {
        if (sub->flags & SUBMIT_COPY_DATA)
            free((void *)sub->data);

        if (sub->completion)
            completion_unref(sub->completion);
    }

    memset(sub, 0, sizeof(*sub));
}
