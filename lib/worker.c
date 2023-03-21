// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>

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
        set_error(w, err);

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

static int add_zero_blocks_before(struct worker *w, struct block *b)
{
    int64_t index;

    /* Don't modify the hash after errors. */
    if (w->error)
        return -1;

    index = w->last_index + w->config->workers;

    while (index < b->index) {
        if (!EVP_DigestUpdate(w->root_ctx, w->config->zero_md, w->config->md_len)) {
            set_error(w, ENOMEM);
            return -1;
        }
        w->last_index = index;
        index += w->config->workers;
    }

    return 0;
}

static int add_data_block(struct worker *w, struct block *b)
{
    unsigned char block_md[EVP_MAX_MD_SIZE];

    /* Don't modify the hash after errors. */
    if (w->error)
        return -1;

    if (!EVP_DigestInit_ex(w->block_ctx, w->config->md, NULL))
        goto error;

    if (!EVP_DigestUpdate(w->block_ctx, b->data, b->len))
        goto error;

    if (!EVP_DigestFinal_ex(w->block_ctx, block_md, NULL))
        goto error;

    if (!EVP_DigestUpdate(w->root_ctx, block_md, w->config->md_len))
        goto error;

    w->last_index = b->index;
    return 0;

error:
    set_error(w, ENOMEM);
    return -1;
}

static void *worker_thread(void *arg)
{
    struct worker *w = arg;

    while (w->running) {
        struct block *block;

        block = pop_block(w);
        if (block == NULL)
            break;

        if (block->last)
            w->running = false;

        add_zero_blocks_before(w, block);

        if (block->len)
            add_data_block(w, block);

        block_free(block);
    }

    drain_queue(w);

    return NULL;
}

int worker_init(struct worker *w, int id, const struct config *config)
{
    int err;

    w->id = id;
    w->config = config;
    w->last_index = id - (int)config->workers;
    w->queue_len = 0;
    w->error = 0;
    w->running = true;
    w->stopped = false;

    w->root_ctx = NULL;
    w->block_ctx = NULL;

    STAILQ_INIT(&w->queue);

    w->root_ctx = EVP_MD_CTX_new();
    if (w->root_ctx == NULL) {
        err = ENOMEM;
        goto fail;
    }

    if (!EVP_DigestInit_ex(w->root_ctx, w->config->md, NULL)) {
        err = ENOMEM;
        goto fail;
    }

    w->block_ctx = EVP_MD_CTX_new();
    if (w->block_ctx == NULL) {
        err = ENOMEM;
        goto fail;
    }

    err = pthread_mutex_init(&w->mutex, NULL);
    if (err)
        goto fail;

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
fail:
    EVP_MD_CTX_free(w->block_ctx);
    EVP_MD_CTX_free(w->root_ctx);

    return err;
}

void worker_destroy(struct worker *w)
{
    pthread_cond_destroy(&w->not_full);
    pthread_cond_destroy(&w->not_empty);
    pthread_mutex_destroy(&w->mutex);

    EVP_MD_CTX_free(w->block_ctx);
    w->block_ctx = NULL;

    EVP_MD_CTX_free(w->root_ctx);
    w->root_ctx = NULL;
}

int worker_update(struct worker *w, struct block *b)
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

    if (b->len > 0 && (b->index % w->config->workers) != w->id) {
        err = EINVAL;
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
    if (b->last)
        w->stopped = true;

    /* The block is owned by the queue now. */
    b = NULL;

    w->queue_len++;
    if (w->queue_len == 1)
        err = pthread_cond_signal(&w->not_empty);

unlock:
    rv = pthread_mutex_unlock(&w->mutex);
    if (rv && !err)
        err = rv;

out:
    if (b)
        block_free(b);

    return err;
}

int worker_stop(struct worker *w)
{
    struct block *b;

    b = block_new(0, 0, NULL);
    if (b == NULL)
        return errno;

    b->last = true;

    return worker_update(w, b);
}

int worker_digest(struct worker *w, unsigned char *md, unsigned int *len)
{
    int err;

    err = pthread_join(w->thread, NULL);
    if (err)
        return err;

    if (w->error)
        return w->error;

    if (!EVP_DigestFinal_ex(w->root_ctx, md, len))
        return ENOMEM;

    return 0;
}
