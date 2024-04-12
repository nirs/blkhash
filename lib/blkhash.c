// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blkhash-internal.h"
#include "blkhash.h"
#include "digest.h"
#include "event.h"
#include "hash-pool.h"
#include "submission.h"
#include "threads.h"
#include "util.h"

/* Number of consecutive zero blocks to batch. */
#define ZERO_BATCH_SIZE 1024

/* Allow large number for testing. */
#define MAX_THREADS 128

struct buffer {
    unsigned char *data;
    size_t len;
    bool zero;
};

/* Used if queue_depth > 0 for blkhash_aio_update calls. accessed both by
 * worker threads and caller threads. */
struct completion_queue {
    pthread_mutex_t mutex;
    struct blkhash_completion *array;
    struct event *event;
    unsigned count;
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

struct blkhash {
    struct config config;

    /* For computing block hashes. */
    struct hash_pool pool;

    struct submission_queue sq;
    struct completion_queue cq;

    /* For keeping partial blocks when user call blkhash_update() with buffer
     * that is not aligned to block size. */
    struct buffer pending;

    /* For computing outer digest from block hashes. */
    struct digest *outer_digest;

    /* Current block index, increased when consuming a data or zero block. */
    int64_t block_index;

    /* The index of the last submitted block. */
    int64_t submitted_index;

    /* The index of the last hashed block. */
    int64_t hashed_index;

    /* Message length incremented on each update or zero. */
    uint64_t message_length;

    /*
     * Number of updates started and not reaped yet. Increased when submitting
     * an async update, and decreased when reaping completions. Modified only
     * by the caller thread.
     */
    unsigned inflight;

    /* The first error. Once set, any operation will fail quickly with this
     * error. */
    int error;

    /* Set when hash is finalized. */
    bool finalized;
};

static const struct blkhash_opts default_opts = {
    .digest_name = "sha256",
    .block_size = 64 * KiB,
    .threads = 4,
    .queue_depth = 0,
};

struct blkhash_opts *blkhash_opts_new(const char *digest_name)
{
    if (digest_name == NULL) {
        errno = EINVAL;
        return NULL;
    }

    struct blkhash_opts *o = malloc(sizeof(*o));
    if (o == NULL)
        return NULL;

    memcpy(o, &default_opts, sizeof(*o));
    o->digest_name = digest_name;

    return o;
}

int blkhash_opts_set_block_size(struct blkhash_opts *o, uint32_t block_size)
{
    if (block_size % 2)
        return EINVAL;

    o->block_size = block_size;
    return 0;
}

int blkhash_opts_set_threads(struct blkhash_opts *o, uint8_t threads)
{
    if (threads < 1 || threads > MAX_THREADS)
        return EINVAL;

    o->threads = threads;
    return 0;
}

int blkhash_opts_set_queue_depth(struct blkhash_opts *o, unsigned queue_depth)
{
    o->queue_depth = queue_depth;
    return 0;
}

const char *blkhash_opts_get_digest_name(struct blkhash_opts *o)
{
    return o->digest_name;
}

uint32_t blkhash_opts_get_block_size(struct blkhash_opts *o)
{
    return o->block_size;
}

uint8_t blkhash_opts_get_threads(struct blkhash_opts *o)
{
    return o->threads;
}

unsigned blkhash_opts_get_queue_depth(struct blkhash_opts *o)
{
    return o->queue_depth;
}

void blkhash_opts_free(struct blkhash_opts *o)
{
    free(o);
}

/* Set the error and return -1. All intenral errors should be handled with
 * this. Public APIs should always return the internal error on failures. */
static inline int set_error(struct blkhash *h, int error)
{
    if (h->error == 0)
        h->error = error;

    return -1;
}

struct blkhash *blkhash_new()
{
    return blkhash_new_opts(&default_opts);
}

struct blkhash *blkhash_new_opts(const struct blkhash_opts *opts)
{
    struct blkhash *h;
    int err;

    h = calloc(1, sizeof(*h));
    if (h == NULL)
        return NULL;

    err = config_init(&h->config, opts);
    if (err)
        goto error;

    err = hash_pool_init(&h->pool, &h->config);
    if (err)
        goto error;

    err = submission_queue_init(&h->sq, h->config.max_submissions);
    if (err)
        goto error;

    if (h->config.queue_depth > 0) {
        err = pthread_mutex_init(&h->cq.mutex, NULL);
        if (err)
            goto error;

        h->cq.array = calloc(h->config.queue_depth, sizeof(*h->cq.array));
        if (h->cq.array == NULL) {
            err = errno;
            goto error;
        }

        err = event_open(&h->cq.event, EVENT_CLOEXEC | EVENT_NONBLOCK);
        if (err) {
            err = -err;
            goto error;
        }
    }

    h->pending.data = calloc(1, h->config.block_size);
    if (h->pending.data == NULL) {
        err = errno;
        goto error;
    }

    err = -digest_create(h->config.digest_name, &h->outer_digest);
    if (err)
        goto error;

    err = -digest_init(h->outer_digest);
    if (err)
        goto error;

    return h;

error:
    blkhash_free(h);
    errno = err;

    return NULL;
}

static int hash_submission(struct blkhash *h, const struct submission *sub)
{
    int err;

    err = submission_error(sub);
    if (err)
        return set_error(h, err);

    /* Add zero blocks befor this block. */
    while (h->hashed_index < sub->index) {
        //fprintf(stderr, "hash zero block %ld\n", h->hashed_index);
        err = -digest_update(h->outer_digest, h->config.zero_md,
                             h->config.md_len);
        if (err)
            return set_error(h, err);

        h->hashed_index++;
    }

    /* Hash this block. */
    if (!submission_is_zero(sub)) {
        //fprintf(stderr, "hash data block %ld\n", sub->index);
        err = -digest_update(h->outer_digest, sub->md, h->config.md_len);
        if (err)
            return set_error(h, err);

        h->hashed_index++;
    }

    return 0;
}

static int hash_message_length(struct blkhash *h)
{
    uint64_t data;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    data = h->message_length;
#else
    data = __builtin_bswap64(h->message_length);
#endif

    int err;

    //printf("message-length: %lu\n", h->message_length);
    err = -digest_update(h->outer_digest, &data, sizeof(data));
    if (err)
        return set_error(h, err);

    return 0;
}

/* If the queue is full, wait until first submission is completed and add it to
 * the outer hash. */
static int maybe_hash_first_submission(struct blkhash *h)
{
    struct submission *sub = NULL;
    int err;

    if (!submission_queue_full(&h->sq))
        return 0;

    err = submission_queue_pop(&h->sq, &sub);
    if (err)
        return set_error(h, err);

    submission_wait(sub);

    if (hash_submission(h, sub))
        return h->error;

    submission_destroy(sub);

    return 0;
}

/* Add all completed submissions to the outer hash. */
static int hash_completed_submissions(struct blkhash *h)
{
    struct submission *sub = NULL;

    while ((sub = submission_queue_first(&h->sq))) {
        if (!submission_is_completed(sub))
            break;

        /* Cannot fail here. */
        submission_queue_pop(&h->sq, NULL);

        if (hash_submission(h, sub))
            return h->error;

        submission_destroy(sub);
    }

    return 0;
}

/* Wait for inflight submissions and add to the outer hash. */
static int hash_inflight_submissions(struct blkhash *h)
{
    struct submission *sub = NULL;

    while ((sub = submission_queue_first(&h->sq))) {
        submission_wait(sub);

        /* Cannot fail here. */
        submission_queue_pop(&h->sq, NULL);

        if (hash_submission(h, sub))
            return h->error;

        submission_destroy(sub);
    }

    return 0;
}

/*
 * Submit one zero length block, adding zero blocks since the last hashed
 * index.
 */
static int submit_zero_block(struct blkhash *h)
{
    struct submission *sub = NULL;
    int err;

    if (maybe_hash_first_submission(h))
        return h->error;

    err = submission_create_zero(h->block_index, &sub);
    if (err)
        return set_error(h, err);

    err = submission_queue_push(&h->sq, sub);
    if (err) {
        submission_destroy(sub);
        return set_error(h, err);
    }

    h->submitted_index = h->block_index;

    if (hash_completed_submissions(h))
        return h->error;

    return 0;
}

/*
 * Submit one data block to the hash pool.
 */
static int submit_data_block(struct blkhash *h, const void *buf, size_t len,
                             struct completion *completion, uint8_t flags)
{
    struct submission *sub = NULL;
    int err;

    if (maybe_hash_first_submission(h))
        return h->error;

    err = submission_create_data(h->block_index, len, buf, completion, flags,
                                 &sub);
    if (err)
        return set_error(h, err);

    err = submission_queue_push(&h->sq, sub);
    if (err) {
        submission_destroy(sub);
        return set_error(h, err);
    }

    err = hash_pool_submit(&h->pool, sub);
    if (err)
        return set_error(h, err);

    h->submitted_index = h->block_index;
    h->block_index++;

    if (hash_completed_submissions(h))
        return h->error;

    return 0;
}

/*
 * Add up to block_size bytes of data to the pending buffer, trying to
 * fill the pending buffer. If the buffer kept pending zeros, convert
 * the pending zeros to data.
 *
 * Return the number of bytes added to the pending buffer.
 */
static size_t add_pending_data(struct blkhash *h, const void *buf, size_t len)
{
    size_t n = MIN(len, h->config.block_size - h->pending.len);

    if (h->pending.zero) {
        /*
         * The buffer contains zeros, convert pending zeros to data.
         */
        memset(h->pending.data, 0, h->pending.len);
        h->pending.zero = false;
    }

    memcpy(h->pending.data + h->pending.len, buf, n);

    h->pending.len += n;

    return n;
}

/*
 * Add up to block_size zero bytes to the pending buffer, trying to fill
 * the pending buffer. If the buffer kept pending data, convert the
 * zeros to data.
 *
 * Return the number of bytes added to the pending buffer.
 */
static size_t add_pending_zeros(struct blkhash *h, size_t len)
{
    size_t n = MIN(len, h->config.block_size - h->pending.len);

    if (h->pending.len == 0) {
        /* The buffer is empty, start collecting pending zeros. */
        h->pending.zero = true;
    } else if (!h->pending.zero) {
        /*
         * The buffer contains some data, convert the zeros to pending data.
         */
        memset(h->pending.data + h->pending.len, 0, n);
    }

    h->pending.len += n;

    return n;
}

/*
 * Consume count zero blocks, sending zero length block if enough zero blocks
 * were consumed since the last data block.
 */
static inline int consume_zero_blocks(struct blkhash *h, size_t count)
{
    h->block_index += count;

    if (h->block_index - h->submitted_index >= ZERO_BATCH_SIZE)
        return submit_zero_block(h);

    return 0;
}

static inline bool is_zero_block(struct blkhash *h, const void *buf, size_t len)
{
    return len == h->config.block_size && is_zero(buf, len);
}

/*
 * Consume len bytes of data from buf. If called with a full block, try
 * to speed the computation by detecting zeros. Detecting zeros in
 * order of magnitude faster compared with computing a message digest.
 */
static int consume_data_block(struct blkhash *h, const void *buf, size_t len,
                              struct completion *completion, uint8_t flags)
{
    /* If we copy data, it worth to eliminate zero blocks now. Otherwise it is
     * better to do this work in the worker. */
    if ((flags & SUBMIT_COPY_DATA) && is_zero_block(h, buf, len)) {
        /* Fast path. */
        return consume_zero_blocks(h, 1);
    } else {
        /* Slow path. */
        return submit_data_block(h, buf, len, completion, flags);
    }
}

/*
 * Consume all pending data or zeros. The pending buffer may contain
 * full or partial block of data or zeros. The pending buffer is
 * cleared after this call.
 */
static int consume_pending(struct blkhash *h, struct completion *completion)
{
    assert(h->pending.len <= h->config.block_size);

    if (h->pending.len == h->config.block_size && h->pending.zero) {
        /* Fast path. */
        if (consume_zero_blocks(h, 1))
            return -1;
    } else {
        /*
         * Slow path if pending is partial block, fast path is pending
         * is full block and pending data is zeros.
         *
         * NOTE: The completion does not protect our pending buffer, so we
         * must use the SUBMIT_COPY_DATA flag.
         */

        if (h->pending.zero) {
            /* Convert partial block of zeros to data. */
            memset(h->pending.data, 0, h->pending.len);
        }

        if (consume_data_block(h, h->pending.data, h->pending.len, completion,
                               SUBMIT_COPY_DATA))
            return -1;
    }

    h->pending.len = 0;
    h->pending.zero = false;
    return 0;
}

static int do_update(struct blkhash *h, const void *buf, size_t len,
                     struct completion *completion, uint8_t flags)
{
    /* Try to fill the pending buffer and consume it. */
    if (h->pending.len > 0) {
        size_t n = add_pending_data(h, buf, len);
        buf += n;
        len -= n;
        if (h->pending.len == h->config.block_size) {
            if (consume_pending(h, completion))
                return h->error;
        }
    }

    /* Consume all full blocks in caller buffer. */
    while (len >= h->config.block_size) {
        if (consume_data_block(h, buf, h->config.block_size, completion,
                               flags))
            return h->error;

        buf += h->config.block_size;
        len -= h->config.block_size;
    }

    /* Copy rest of the data to the internal buffer. */
    if (len > 0)
        add_pending_data(h, buf, len);

    return 0;
}

int blkhash_update(struct blkhash *h, const void *buf, size_t len)
{
    if (h->error)
        return h->error;

    h->message_length += len;

    /* We copy user data to simplify the interface. Users that want higher
     * performance should use the async interface. */
    return do_update(h, buf, len, NULL, SUBMIT_COPY_DATA);
}

static void update_completed(struct blkhash *h, void *user_data, int error)
{
    struct blkhash_completion *c;
    int err;
    bool was_empty;

    mutex_lock(&h->cq.mutex);

    was_empty = h->cq.count == 0;
    assert(h->cq.count < h->config.queue_depth);
    c = &h->cq.array[h->cq.count++];
    c->user_data = user_data;
    c->error = error;

    mutex_unlock(&h->cq.mutex);

    if (was_empty) {
        /* If we cannot noitify, the caller may get stuck waiting for
         * completions.  Setting the error will fail the next request. */
        err = event_signal(h->cq.event);
        if (err)
            set_error(h, -err);
    }
}

int blkhash_aio_update(struct blkhash *h, const void *buf, size_t len,
                       void *user_data)
{
    struct completion *completion;

    if (h->error)
        return h->error;

    /* Accessed only by caller thread, no locking needed. */
    if (h->inflight >= h->config.queue_depth)
        return EAGAIN;

    h->inflight++;

    completion = completion_new(update_completed, h, user_data);
    if (completion == NULL) {
        set_error(h, errno);
        return h->error;
    }

    h->message_length += len;

    /* We don't copy user data, and the user must wait for completion before
     * using or freeing the buffer. Users that want simpler interface should
     * use simpler the blocking API. */
    if (do_update(h, buf, len, completion, 0))
        completion_set_error(completion, h->error);

    completion_unref(completion);

    return 0;
}

int blkhash_aio_completion_fd(struct blkhash *h)
{
    if (h->cq.event == NULL)
        return -ENOTSUP;

    return h->cq.event->read_fd;
}

int blkhash_aio_completions(struct blkhash *h, struct blkhash_completion *out,
                            unsigned count)
{
    bool have_more = false;

    if (h->error)
        return -h->error;

    mutex_lock(&h->cq.mutex);

    count = MIN(count, h->cq.count);
    if (count > 0) {
        size_t bytes = count * sizeof(*h->cq.array);
        memcpy(out, h->cq.array, bytes);
        h->cq.count -= count;

        if (h->cq.count > 0) {
            /* There is no reason to call with count < queue_depth, so this
             * should not happen. */
            size_t bytes = h->cq.count * sizeof(*h->cq.array);
            memmove(&h->cq.array[0], &h->cq.array[count], bytes);
            have_more = true;
        }
    }

    mutex_unlock(&h->cq.mutex);

    /* Accessed only by caller thread, no locking needed. */
    if (count > 0) {
        assert(h->inflight >= count);
        h->inflight -= count;
    }

    if (have_more) {
        /* If not all completions consumed (unlikely), singal the completion fd
         * so the user will get a notification on the next poll. */
        int err = event_signal(h->cq.event);
        if (err) {
            set_error(h, -err);
            return err;
        }
    }

    return count;
}

int blkhash_zero(struct blkhash *h, size_t len)
{
    if (h->error)
        return h->error;

    h->message_length += len;

    /* Try to fill the pending buffer and consume it. */
    if (h->pending.len > 0) {
        len -= add_pending_zeros(h, len);
        if (h->pending.len == h->config.block_size) {
            if (consume_pending(h, NULL))
                return h->error;
        }
    }

    /* Consume all full zero blocks. */
    if (len >= h->config.block_size) {
        if (consume_zero_blocks(h, len / h->config.block_size))
            return h->error;

        len %= h->config.block_size;
    }

    /* Save the rest in the pending buffer. */
    if (len > 0)
        add_pending_zeros(h, len);

    return 0;
}

int blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len)
{
    if (h->finalized)
        return EINVAL;

    h->finalized = true;

    if (h->pending.len > 0) {
        if (consume_pending(h, NULL))
            return h->error;
    } else {
        if (submit_zero_block(h))
            return h->error;
    }

    if (hash_inflight_submissions(h))
        return h->error;

    if (hash_message_length(h))
        return h->error;

    return -digest_final(h->outer_digest, md_value, md_len);
}

void blkhash_free(struct blkhash *h)
{
    if (h == NULL)
        return;

    digest_destroy(h->outer_digest);
    free(h->pending.data);

    if (h->config.queue_depth) {
        event_close(h->cq.event);
        free(h->cq.array);
        mutex_destroy(&h->cq.mutex);
    }

    submission_queue_destroy(&h->sq);
    hash_pool_destroy(&h->pool);

    free(h);
}
