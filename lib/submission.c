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

int submission_init_data(struct submission *sub, int64_t index, uint32_t len,
                         const void *data, struct completion *completion,
                         uint8_t flags)
{
    int err = 0;

    err = sem_init(&sub->sem, 0, 0);
    if (err)
        return errno;

    sub->type = DATA;
    sub->completion = completion;
    sub->data = data;
    sub->index = index;
    sub->len = len;
    sub->error = 0;
    sub->flags = flags;

    if (flags & SUBMIT_COPY_DATA) {
        err = copy_data(sub);
        if (err)
            goto error;
    }

    if (sub->completion)
        completion_ref(sub->completion);

    return 0;

error:
    if (sem_destroy(&sub->sem))
        ABORTF("sem_destroy: %s", strerror(errno));

    return err;
}

int submission_init_zero(struct submission *sub, int64_t index)
{
    int err;

    /* Initialize to 1 so submission_is_completed() and submssion_wait() will
     * return imediately. */
    err = sem_init(&sub->sem, 0, 1);
    if (err)
        return errno;

    sub->type = ZERO;
    sub->index = index;

    sub->data = NULL;
    sub->completion = NULL;
    sub->len = 0;
    sub->error = 0;
    sub->flags = 0;

    return 0;
}

void submission_set_error(struct submission *sub, int error)
{
    __atomic_store_n(&sub->error, error, __ATOMIC_RELEASE);
    if (sub->completion)
        completion_set_error(sub->completion, error);
}

int submission_error(const struct submission *sub)
{
    return __atomic_load_n(&sub->error, __ATOMIC_ACQUIRE);
}

void submission_complete(struct submission *sub)
{
    if (sub->flags & SUBMIT_COPY_DATA) {
        free((void *)sub->data);
        sub->data = NULL;
    }

    if (sub->completion) {
        completion_unref(sub->completion);
        sub->completion = NULL;
    }

    /* Cannot fail in current code. */
    if (sem_post(&sub->sem))
        ABORTF("sem_post: %s", strerror(errno));
}

int submission_wait(struct submission *sub)
{
    int err;

    do {
        err = sem_wait(&sub->sem);
    } while (err != 0 && errno == EINTR);

    if (err != 0)
        return errno;

    return 0;
}

int submission_queue_pop(struct submission_queue *sq, struct submission **out)
{
    if (sq->len == 0)
        return ENOENT;

    if (out)
        *out = &sq->ring[sq->head];

    sq->head = (sq->head + 1) % sq->size;
    sq->len--;

    return 0;
}

int submission_queue_push(struct submission_queue *sq, struct submission **out)
{
    if (sq->len == sq->size)
        return ENOBUFS;

    *out = &sq->ring[sq->tail];

    sq->tail = (sq->tail + 1) % sq->size;
    sq->len++;

    return 0;
}

void submission_destroy(struct submission *sub)
{
    /* If not completed, clean up now. */
    if (sub->flags & SUBMIT_COPY_DATA)
        free((void *)sub->data);

    /* Cannot fail in current code. */
    if (sem_destroy(&sub->sem))
        ABORTF("sem_destroy: %s", strerror(errno));

    memset(sub, 0, sizeof(*sub));
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
