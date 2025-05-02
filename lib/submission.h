// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef SUBMISSION_H
#define SUBMISSION_H

#include <errno.h>
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

struct submission {
    unsigned char md[BLKHASH_MAX_MD_SIZE];

    /* Completion for DATA submission, used to wait until all submissions are
     * handled by the workers. */
    struct completion *completion;

    /* Data for DATA submission. */
    const void *data;

    int64_t index;

    /* Length of data for DATA submission. */
    uint32_t len;

    int error;

    /* Block is unallocated or full of zeros. */
    bool zero;

    /* Processing was completed. */
    bool completed;

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

static inline void submission_set_zero(struct submission *sub)
{
    sub->zero = true;
}

static inline bool submission_is_zero(const struct submission *sub)
{
    return sub->zero;
}

static inline void submission_set_error(struct submission *sub, int error)
{
    sub->error = error;

    if (sub->completion)
        completion_set_error(sub->completion, error);
}

static inline int submission_error(const struct submission *sub)
{
    return sub->error;
}

static inline void submission_complete(struct submission *sub)
{
    if (sub->completion) {
        completion_unref(sub->completion);
        sub->completion = NULL;
    }

    /*
     * Synchronize with the fence in submission_is_completed().  No reads or
     * writes in the current thread can be reordered after this store.
     *
     * See https://en.cppreference.com/w/c/atomic/memory_order#Constants
     */
    __atomic_store_n(&sub->completed, true, __ATOMIC_RELEASE);
}

static inline bool submission_is_completed(const struct submission *sub)
{
    bool completed = __atomic_load_n(&sub->completed, __ATOMIC_RELAXED);

    /*
     * Synchronize with the store in submission_complete().  All memory writes
     * (including non-atomic and relaxed atomic) that happened-before the
     * atomic store in submission_complete() become visible side-effects in
     * this thread.
     *
     * See https://en.cppreference.com/w/c/atomic/memory_order#Release-Acquire_ordering
     */
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    return completed;
}

static inline void submission_wait(const struct submission *sub)
{
    while (!submission_is_completed(sub))
        ;
}

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
