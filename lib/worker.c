// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>

#include "blkhash-internal.h"
#include "threads.h"

/* Maximum number of submissions to queue per worker. */
#define QUEUE_SIZE 16

static inline void set_error(struct worker *w, int error)
{
    /* Keep the first error. */
    if (w->error == 0) {
        w->error = error;
        w->running = false;
    }
}

static struct submission *pop_submission(struct worker *w)
{
    struct submission *sub;
    bool was_full = false;

    mutex_lock(&w->mutex);

    while (STAILQ_EMPTY(&w->queue))
        cond_wait(&w->not_empty, &w->mutex);

    sub = STAILQ_FIRST(&w->queue);
    STAILQ_REMOVE_HEAD(&w->queue, entry);

    was_full = w->queue_len == QUEUE_SIZE;
    w->queue_len--;
    if (was_full)
        cond_signal(&w->not_full);

    mutex_unlock(&w->mutex);

    return sub;
}

/* Called during cleanup - ignore errors. */
static void drain_queue(struct worker *w)
{
    bool was_full = false;

    mutex_lock(&w->mutex);

    while (!STAILQ_EMPTY(&w->queue)) {
        struct submission *sub;

        sub = STAILQ_FIRST(&w->queue);
        STAILQ_REMOVE_HEAD(&w->queue, entry);
        submission_free(sub);
    }

    was_full = w->queue_len == QUEUE_SIZE;
    w->queue_len = 0;
    if (was_full)
        cond_signal(&w->not_full);

    mutex_unlock(&w->mutex);
}

static void *worker_thread(void *arg)
{
    struct worker *w = arg;

    while (w->running) {
        struct submission *sub;

        sub = pop_submission(w);
        if (sub == NULL)
            break;

        if (sub->type == STOP)
            w->running = false;
        else {
            int err = stream_update(sub->stream, sub);
            if (err)
                set_error(w, err);
        }

        submission_free(sub);
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
    cond_destroy(&w->not_full);
    cond_destroy(&w->not_empty);
    mutex_destroy(&w->mutex);
}

int worker_submit(struct worker *w, struct submission *sub)
{
    int err = 0;

    mutex_lock(&w->mutex);

    /* Nothing will be submitted after a worker was stopped. */
    if (w->stopped) {
        err = EPERM;
        goto out;
    }

    /* The submission will leak if the worker failed. */
    if (w->error) {
        err = sub->type == STOP ? 0: w->error;
        goto out;
    }

    while (w->queue_len >= QUEUE_SIZE)
        cond_wait(&w->not_full, &w->mutex);

    STAILQ_INSERT_TAIL(&w->queue, sub, entry);
    w->queue_len++;

    /* Ensure that nothing is submitted after the last submission. */
    if (sub->type == STOP)
        w->stopped = true;

    /* The submission is owned by the queue now. */
    sub = NULL;

    if (w->queue_len == 1)
        cond_signal(&w->not_empty);

out:
    mutex_unlock(&w->mutex);
    if (sub)
        submission_free(sub);
    return err;
}

int worker_stop(struct worker *w)
{
    struct submission *sub;

    sub = submission_new_stop();
    if (sub == NULL)
        return errno;

    return worker_submit(w, sub);
}

int worker_join(struct worker *w)
{
    int err;

    err = pthread_join(w->thread, NULL);
    if (err)
        return err;

    return w->error;
}
