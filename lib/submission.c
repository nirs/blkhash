// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdint.h>
#include <stdlib.h>

#include "submission.h"

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

int submission_create_data(int64_t index, uint32_t len, const void *data,
                           struct completion *completion, uint8_t flags,
                           struct submission **out)
{
    struct submission *sub;
    int err;

    sub = malloc(sizeof(*sub));
    if (sub == NULL)
        return errno;

    sub->completion = completion;
    sub->data = data;
    sub->index = index;
    sub->len = len;
    sub->error = 0;
    sub->zero = false;
    sub->completed = false;
    sub->flags = flags;

    if (flags & SUBMIT_COPY_DATA) {
        err = copy_data(sub);
        if (err)
            goto error;
    }

    if (sub->completion)
        completion_ref(sub->completion);

    *out = sub;
    return 0;

error:
    free(sub);
    return err;
}

int submission_create_zero(int64_t index, struct submission **out)
{
    struct submission *sub;

    sub = malloc(sizeof(*sub));
    if (sub == NULL)
        return errno;

    sub->completion = NULL;
    sub->data = NULL;
    sub->index = index;
    sub->len = 0;
    sub->error = 0;
    sub->zero = true;
    sub->completed = true;
    sub->flags = 0;

    *out = sub;
    return 0;
}

void submission_destroy(struct submission *sub)
{
    if (sub == NULL)
        return;

    if (sub->data && sub->flags & SUBMIT_COPY_DATA)
        free((void *)sub->data);

    free(sub);
}

int submission_queue_init(struct submission_queue *sq, unsigned size)
{
    sq->ring = malloc(size * sizeof(*sq->ring));
    if (sq->ring == NULL)
        return errno;

    sq->size = size;
    sq->len = 0;
    sq->head = 0;
    sq->tail = 0;

    return 0;
}

int submission_queue_pop(struct submission_queue *sq, struct submission **out)
{
    if (sq->len == 0)
        return ENOENT;

    if (out)
        *out = sq->ring[sq->head];

    sq->head = (sq->head + 1) % sq->size;
    sq->len--;

    return 0;
}

int submission_queue_push(struct submission_queue *sq, struct submission *sub)
{
    if (sq->len == sq->size)
        return ENOBUFS;

    sq->ring[sq->tail] = sub;

    sq->tail = (sq->tail + 1) % sq->size;
    sq->len++;

    return 0;
}

void submission_queue_destroy(struct submission_queue *sq)
{
    struct submission *sub;

    while (sq->len > 0) {
        submission_queue_pop(sq, &sub);
        submission_destroy(sub);
    }

    free(sq->ring);
    memset(sq, 0, sizeof(*sq));
}
