// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>

#include "blkhash-internal.h"
#include "digest.h"
#include "hash-pool.h"
#include "submission.h"
#include "threads.h"

static struct submission *STOP = (struct submission *)-1;

static void set_error(struct hash_pool *p, int error)
{
    mutex_lock(&p->mutex);

    /* Keep the first error. */
    if (p->error == 0)
        p->error = error;

    mutex_unlock(&p->mutex);
}

static inline void push_unlocked(struct hash_pool *p, struct submission *sub)
{
    p->queue[p->queue_tail] = sub;
    p->queue_tail = (p->queue_tail + 1) % p->queue_size;
    p->queue_len++;
}

static inline struct submission *pop_unlocked(struct hash_pool *p)
{
    struct submission *sub;

    sub = p->queue[p->queue_head];
    p->queue_head = (p->queue_head + 1) % p->queue_size;
    p->queue_len--;

    return sub;
}

static struct submission *wait_for_work(struct hash_pool *p)
{
    struct submission *sub;

    mutex_lock(&p->mutex);

    while (p->queue_len == 0) {
        p->workers_idle++;
        cond_wait(&p->not_empty, &p->mutex);
        p->workers_idle--;
    }

    sub = pop_unlocked(p);

    mutex_unlock(&p->mutex);

    return sub;
}

static bool is_zero_block(struct hash_pool *p, const struct submission *sub)
{
    return !(sub->flags & SUBMIT_COPY_DATA) &&
        sub->len == p->config->block_size &&
        is_zero(sub->data, sub->len);
}

static void compute_block_digest(struct submission *sub, struct digest *digest)
{
    int err;

    err = -digest_init(digest);
    if (err)
        goto error;

    err = -digest_update(digest, sub->data, sub->len);
    if (err)
        goto error;

    err = -digest_final(digest, sub->md, NULL);
    if (err)
        goto error;

    return;

error:
    submission_set_error(sub, err);
}

static void *worker_thread(void *arg)
{
    struct hash_pool *p = arg;
    struct submission *sub;
    struct digest *digest;
    int err;

    err = digest_create(p->config->digest_name, &digest);
    if (err) {
        set_error(p, err);
        return NULL;
    }

    for (;;) {
        sub = wait_for_work(p);
        if (sub == STOP)
            break;

        if (is_zero_block(p, sub))
            submission_set_zero(sub);
        else
            compute_block_digest(sub, digest);

        submission_complete(sub);
    }

    digest_destroy(digest);

    return NULL;
}

static void stop_workers(struct hash_pool *p)
{
    mutex_lock(&p->mutex);

    /* New sumissions will fail with EIO. */
    p->stopped = true;

    /* Clear the queue, failing all queued submissions. */
    while (p->queue_len > 0) {
        struct submission *sub = pop_unlocked(p);
        submission_set_error(sub, EIO);
        submission_complete(sub);
    }

    /* Add STOP submission for every worker. */
    for (unsigned i = 0; i < p->workers_count; i++) {
        push_unlocked(p, STOP);
    }

    /* Wake up all workers and wait until they terminate. */
    cond_broadcast(&p->not_empty);

    for (unsigned i = 0; i < p->workers_count; i++) {
        mutex_unlock(&p->mutex);
        pthread_join(p->workers[i], NULL);
        mutex_lock(&p->mutex);
    }

    mutex_unlock(&p->mutex);
}

int hash_pool_init(struct hash_pool *p, const struct config *config)
{
    int err;

    p->config = config;
    p->queue_size = config->max_submissions;
    p->queue_len = 0;
    p->queue_head = 0;
    p->queue_tail = 0;
    p->workers_count = 0;
    p->workers_idle = 0;
    p->stopped = false;
    p->error = 0;

    p->queue = calloc(p->queue_size, sizeof(*p->queue));
    if (p->queue == NULL)
        return errno;

    p->workers = calloc(config->workers, sizeof(*p->workers));
    if (p->workers == NULL) {
        err = errno;
        goto fail_workers;
    }

    err = pthread_mutex_init(&p->mutex, NULL);
    if (err)
        goto fail_mutex;

    err = pthread_cond_init(&p->not_empty, NULL);
    if (err)
        goto fail_not_empty;

    for (unsigned i = 0; i < config->workers; i++) {
        err = pthread_create(&p->workers[i], NULL, worker_thread, p);
        if (err)
            goto fail_thread;

        p->workers_count++;
    }

    return 0;

fail_thread:
    stop_workers(p);
    pthread_cond_destroy(&p->not_empty);
fail_not_empty:
    pthread_mutex_destroy(&p->mutex);
fail_mutex:
    free(p->workers);
fail_workers:
    free(p->queue);

    return err;
}

static inline unsigned active_workers(struct hash_pool *p)
{
    return p->workers_count - p->workers_idle;
}

/*
 * If the queue does not have enough work it is better to let idle workers
 * sleep little bit to avoid waking up a worker for for every cycle.
 */
static inline bool need_wakeup(struct hash_pool *p)
{
    return p->workers_idle > 0 && p->queue_len > active_workers(p);
}

int hash_pool_submit(struct hash_pool *p, struct submission *sub)
{
    int err = 0;

    mutex_lock(&p->mutex);

    if (p->stopped) {
        err = EPERM;
        goto out;
    }

    if (p->error) {
        err = p->error;
        goto out;
    }

    /* The queue can never be full since the submitter waits until the
     * submission queue is not full. */
    if (p->queue_len == p->queue_size) {
        err = ENOBUFS;
        goto out;
    }

    push_unlocked(p, sub);

    if (need_wakeup(p))
        cond_signal(&p->not_empty);

out:
    mutex_unlock(&p->mutex);

    if (err) {
        submission_set_error(sub, err);
        submission_complete(sub);
    }

    return err;
}

int hash_pool_destroy(struct hash_pool *p)
{
    stop_workers(p);

    pthread_cond_destroy(&p->not_empty);
    pthread_mutex_destroy(&p->mutex);
    free(p->workers);
    free(p->queue);

    return 0;
}
