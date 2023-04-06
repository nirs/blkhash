// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <string.h>

#include "blkhash-internal.h"

/* Maximum number of blocks to queue per worker. */
#define MAX_BLOCKS 16

static inline void set_error(struct worker *w, int error)
{
    /* Keep the first error. */
    if (w->error == 0) {
        w->error = error;
        w->running = false;
    }
}

static struct block *pop_block(struct worker *w)
{
    bool was_full;
    struct block *block = NULL;
    int err;

    err = pthread_mutex_lock(&w->mutex);
    if (err) {
        set_error(w, err);
        return NULL;
    }

    while (STAILQ_EMPTY(&w->queue)) {
        err = pthread_cond_wait(&w->not_empty, &w->mutex);
        if (err) {
            set_error(w, err);
            goto out;
        }
    }

    block = STAILQ_FIRST(&w->queue);
    STAILQ_REMOVE_HEAD(&w->queue, entry);

    was_full = w->queue_len == MAX_BLOCKS;
    w->queue_len--;

    if (was_full) {
        err = pthread_cond_signal(&w->not_full);
        if (err)
            set_error(w, err);
    }

out:
    err = pthread_mutex_unlock(&w->mutex);
    if (err)
        ABORTF("pthread_mutex_unlock: %s", strerror(err));

    if (w->error) {
        block_free(block);
        return NULL;
    }

    return block;
}

/* Called during cleanup - ignore errors. */
static void drain_queue(struct worker *w)
{
    struct block *block;

    pthread_mutex_lock(&w->mutex);

    while (!STAILQ_EMPTY(&w->queue)) {
        block = STAILQ_FIRST(&w->queue);
        STAILQ_REMOVE_HEAD(&w->queue, entry);
        block_free(block);
    }

    if (w->queue_len == MAX_BLOCKS)
        pthread_cond_signal(&w->not_full);

    w->queue_len = 0;

    pthread_mutex_unlock(&w->mutex);
}

static void *worker_thread(void *arg)
{
    struct worker *w = arg;

    while (w->running) {
        struct block *block;

        block = pop_block(w);
        if (block == NULL)
            break;

        if (block->type == STOP)
            w->running = false;
        else {
            int err = stream_update(block->stream, block);
            if (err)
                set_error(w, err);
        }

        block_free(block);
    }

    drain_queue(w);

    return NULL;
}

int worker_init(struct worker *w)
{
    int err;

    w->queue_len = 0;
    w->error = 0;
    w->running = true;
    w->stopped = false;

    STAILQ_INIT(&w->queue);

    err = pthread_mutex_init(&w->mutex, NULL);
    if (err)
        return err;

    err = pthread_cond_init(&w->not_empty, NULL);
    if (err)
        goto fail_not_empty;

    err = pthread_cond_init(&w->not_full, NULL);
    if (err)
        goto fail_not_full;

    err = pthread_create(&w->thread, NULL, worker_thread, w);
    if (err)
        goto fail_thread;

    return 0;

fail_thread:
    pthread_cond_destroy(&w->not_full);
fail_not_full:
    pthread_cond_destroy(&w->not_empty);
fail_not_empty:
    pthread_mutex_destroy(&w->mutex);

    return err;
}

void worker_destroy(struct worker *w)
{
    pthread_cond_destroy(&w->not_full);
    pthread_cond_destroy(&w->not_empty);
    pthread_mutex_destroy(&w->mutex);
}

int worker_submit(struct worker *w, struct block *b)
{
    int err = 0;
    int rv;

    err = pthread_mutex_lock(&w->mutex);
    if (err)
        goto out;

    if (w->stopped) {
        err = EPERM;
        goto unlock;
    }

    /* Nothing will be submitted after a worker was stopped. */
    if (w->stopped) {
        err = EPERM;
        goto unlock;
    }

    /* The block will leak if the worker failed. */
    if (w->error) {
        err = w->error;
        goto unlock;
    }

    while (w->queue_len >= MAX_BLOCKS) {
        err = pthread_cond_wait(&w->not_full, &w->mutex);
        if (err)
            goto unlock;
    }

    STAILQ_INSERT_TAIL(&w->queue, b, entry);

    /* Ensure that nothing is submitted after the last block. */
    if (b->type == STOP)
        w->stopped = true;

    /* The block is owned by the queue now. */
    b = NULL;

    w->queue_len++;
    if (w->queue_len == 1)
        err = pthread_cond_signal(&w->not_empty);

unlock:
    rv = pthread_mutex_unlock(&w->mutex);
    if (rv)
        ABORTF("pthread_mutex_unlock: %s", strerror(err));

out:
    if (b)
        block_free(b);

    return err;
}

int worker_stop(struct worker *w)
{
    struct block *b;

    b = block_new(STOP, NULL, 0, 0, NULL);
    if (b == NULL)
        return errno;

    return worker_submit(w, b);
}

int worker_join(struct worker *w)
{
    int err;

    err = pthread_join(w->thread, NULL);
    if (err)
        return err;

    return w->error;
}
