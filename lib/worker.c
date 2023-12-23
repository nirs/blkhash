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

static void pop_submission(struct worker *w, struct submission *sub)
{
    bool was_full = false;

    mutex_lock(&w->mutex);

    while (w->queue_len == 0)
        cond_wait(&w->not_empty, &w->mutex);

    was_full = w->queue_len == QUEUE_SIZE;

    memcpy(sub, &w->queue[w->queue_head], sizeof(*sub));

    w->queue_head = (w->queue_head + 1) % QUEUE_SIZE;
    w->queue_len--;

    if (was_full)
        cond_signal(&w->not_full);

    mutex_unlock(&w->mutex);
}

/* Called during cleanup - ignore errors. */
static void drain_queue(struct worker *w)
{
    bool was_full = false;

    mutex_lock(&w->mutex);

    was_full = w->queue_len == QUEUE_SIZE;

    while (w->queue_len > 0) {
        struct submission *sub = &w->queue[w->queue_head];

        submission_set_error(sub, w->error ? w->error : EIO);
        submission_destroy(sub);

        w->queue_head = (w->queue_head + 1) % QUEUE_SIZE;
        w->queue_len--;
    }

    if (was_full)
        cond_signal(&w->not_full);

    mutex_unlock(&w->mutex);
}

static void *worker_thread(void *arg)
{
    struct worker *w = arg;
    struct submission sub = {0};

    while (w->running) {
        pop_submission(w, &sub);

        if (sub.type == STOP) {
            w->running = false;
        } else {
            int err = stream_update(sub.stream, &sub);
            if (err)
                set_error(w, err);
        }

        submission_destroy(&sub);
    }

    drain_queue(w);

    return NULL;
}

int worker_init(struct worker *w)
{
    int err;

    w->queue_len = 0;
    w->queue_head = 0;
    w->queue_tail = 0;
    w->error = 0;
    w->running = true;
    w->stopped = false;

    w->queue = calloc(QUEUE_SIZE, sizeof(*w->queue));
    if (w->queue == NULL)
        return errno;

    err = pthread_mutex_init(&w->mutex, NULL);
    if (err)
        goto fail_mutex;

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
fail_mutex:
    free(w->queue);

    return err;
}

void worker_destroy(struct worker *w)
{
    cond_destroy(&w->not_full);
    cond_destroy(&w->not_empty);
    mutex_destroy(&w->mutex);
    free(w->queue);
}

int worker_submit(struct worker *w, struct submission *sub)
{
    int err = 0;

    mutex_lock(&w->mutex);

    if (w->stopped) {
        err = EPERM;
        goto out;
    }

    /* Stopping must not fail even if the worker has failed. */
    if (sub->type == STOP) {
        w->stopped = true;
    } else if (w->error) {
        err = w->error;
        goto out;
    }

    while (w->queue_len >= QUEUE_SIZE)
        cond_wait(&w->not_full, &w->mutex);

    memcpy(&w->queue[w->queue_tail], sub, sizeof(*sub));

    w->queue_tail = (w->queue_tail + 1) % QUEUE_SIZE;
    w->queue_len++;

    if (w->queue_len == 1)
        cond_signal(&w->not_empty);

out:
    mutex_unlock(&w->mutex);

    /* If the submission failed set the error and destroy it to signal completion. */
    if (err) {
        submission_set_error(sub, err);
        submission_destroy(sub);
    }

    return err;
}

int worker_stop(struct worker *w)
{
    struct submission sub = {.type=STOP};
    return worker_submit(w, &sub);
}

int worker_join(struct worker *w)
{
    int err;

    err = pthread_join(w->thread, NULL);
    if (err)
        return err;

    return w->error;
}
