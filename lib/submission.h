// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef SUBMISSION_H
#define SUBMISSION_H

#include <errno.h>
#include <semaphore.h>
#include <stdbool.h>
#include <string.h>

#include "blkhash-config.h"
#include "blkhash-internal.h"

/*
 * Copy data from user buffer into the submission. When not set
 * blkhash_update() can return immediately and the user can use the buffer for
 * the next call or free it.  When set, the caller must wait using completion
 * callback and must not modify the buffer before receiving the completion
 * callback.
 */
#define SUBMIT_COPY_DATA 0x1

/* XXX Replace with boolean flag. */
enum submission_type {DATA, ZERO};

struct submission {
    unsigned char md[BLKHASH_MAX_MD_SIZE];

    /* For signalling and waiting for completion. */
    sem_t sem;

    /* Completion for DATA submission, used to wait until all submissions are
     * handled by the workers. */
    struct completion *completion;

    /* Data for DATA submission. */
    const void *data;

    int64_t index;

    /* Length of data for DATA submission. */
    uint32_t len;

    int error;
    uint8_t type;
    uint8_t flags;

    /* Align to avoid false sharing between workers. */
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

struct submission_queue {
    struct submission **ring;
    unsigned size;
    unsigned len;
    unsigned head;
    unsigned tail;
};

int submission_create_data(int64_t index, uint32_t len, const void *data,
                           struct completion *completion, uint8_t flags,
                           struct submission **out);

int submission_create_zero(int64_t index, struct submission **out);

void submission_set_error(struct submission *sub, int error);

int submission_error(const struct submission *sub);

void submission_complete(struct submission *sub);

/* Called many times for the first submission in the queue. */
static inline bool submission_is_completed(struct submission *sub)
{
    int err;

    do {
        err = sem_trywait(&sub->sem);
    } while (err != 0 && errno == EINTR);

    if (err != 0) {
        if (errno != EAGAIN)
            ABORTF("sem_trywait: %s", strerror(errno));

        return false;
    }

    return true;
}

int submission_wait(struct submission *sub);

void submission_destroy(struct submission *sub);

int submission_queue_init(struct submission_queue *sq, unsigned size);

static inline struct submission *submission_queue_first(struct submission_queue *sq)
{
    return sq->len > 0 ? sq->ring[sq->head] : NULL;
}

static inline bool submission_queue_full(struct submission_queue *sq)
{
    return sq->len == sq->size;
}

int submission_queue_pop(struct submission_queue *sq, struct submission **out);

int submission_queue_push(struct submission_queue *sq, struct submission *sub);

void submission_queue_destroy(struct submission_queue *sq);

#endif /* SUBMISSION_H */
