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

struct submission *submission_new_data(struct stream *stream, int64_t index,
                                       size_t len, const void *data,
                                       struct completion *completion,
                                       uint8_t flags)
{
    struct submission *sub;

    sub = malloc(sizeof(*sub));
    if (sub == NULL)
        return NULL;

    sub->type = DATA;
    sub->stream = stream;
    sub->completion = completion;
    sub->data = data;
    sub->index = index;
    sub->len = len;
    sub->flags = flags;

    if (flags & SUBMIT_COPY_DATA) {
        int err = copy_data(sub);
        if (err) {
            free(sub);
            errno = err;
            return NULL;
        }
    }

    if (sub->completion)
        completion_ref(sub->completion);

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
    if (sub == NULL)
        return;

    if (sub->type == DATA) {
        if (sub->flags & SUBMIT_COPY_DATA)
            free((void *)sub->data);

        if (sub->completion)
            completion_unref(sub->completion);
    }

    free(sub);
}
