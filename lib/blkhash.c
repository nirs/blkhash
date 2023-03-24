// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "blkhash.h"
#include "blkhash-internal.h"
#include "util.h"

/* Number of consecutive zero blocks per stream to consume before submitting
 * zero length block to all streams. */
#define STREAM_ZERO_BATCH_SIZE (16 * 1024)

struct blkhash {
    struct config config;

    /* The streams computing internal hashes. */
    struct stream *streams;
    unsigned streams_count;

    /* Workers processing streams. */
    struct worker *workers;
    unsigned workers_count;

    /* For computing root digest from the streams hashes. */
    EVP_MD_CTX *root_ctx;

    /* For keeping partial blocks when user call blkhash_update() with buffer
     * that is not aligned to block size. */
    unsigned char *pending;
    size_t pending_len;
    bool pending_zero;

    /* Current block index, increased when consuming a data or zero block. */
    int64_t block_index;

    /* The index of the last submitted block. */
    int64_t update_index;

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
    .streams = 4,
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

int blkhash_opts_set_block_size(struct blkhash_opts *o, size_t block_size)
{
    if (block_size % 2)
        return EINVAL;

    o->block_size = block_size;
    return 0;
}

int blkhash_opts_set_threads(struct blkhash_opts *o, uint8_t threads)
{
    if (threads < 1 || threads > o->streams)
        return EINVAL;

    o->threads = threads;
    return 0;
}

int blkhash_opts_set_streams(struct blkhash_opts *o, uint8_t streams)
{
    if (streams < o->threads)
        return EINVAL;

    o->streams = streams;
    return 0;
}

const char *blkhash_opts_get_digest_name(struct blkhash_opts *o)
{
    return o->digest_name;
}

size_t blkhash_opts_get_block_size(struct blkhash_opts *o)
{
    return o->block_size;
}

uint8_t blkhash_opts_get_threads(struct blkhash_opts *o)
{
    return o->threads;
}

uint8_t blkhash_opts_get_streams(struct blkhash_opts *o)
{
    return o->streams;
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

    h->streams = calloc(h->config.streams, sizeof(*h->streams));
    if (h->streams == NULL) {
        err = errno;
        goto error;
    }

    while (h->streams_count < h->config.streams) {
        err = stream_init(&h->streams[h->streams_count], h->streams_count, &h->config);
        if (err)
            goto error;

        h->streams_count++;
    }

    h->workers = calloc(h->config.workers, sizeof(*h->workers));
    if (h->workers == NULL) {
        err = errno;
        goto error;
    }

    while (h->workers_count < h->config.workers) {
        err = worker_init(&h->workers[h->workers_count]);
        if (err)
            goto error;

        h->workers_count++;
    }

    h->root_ctx = EVP_MD_CTX_new();
    if (h->root_ctx == NULL) {
        err = ENOMEM;
        goto error;
    }

    if (!EVP_DigestInit_ex(h->root_ctx, h->config.md, NULL)) {
        err = ENOMEM;
        goto error;
    }

    h->pending = calloc(1, h->config.block_size);
    if (h->pending == NULL) {
        err = errno;
        goto error;
    }

    return h;

error:
    blkhash_free(h);
    errno = err;

    return NULL;
}

static inline struct stream *stream_for_block(struct blkhash *h, int64_t block_index)
{
    int stream_index = block_index % h->config.streams;
    return &h->streams[stream_index];
}

static inline struct worker *worker_for_stream(struct blkhash *h, int stream_index)
{
    int worker_index = stream_index % h->config.workers;
    return &h->workers[worker_index];
}

/*
 * Submit one zero length block to all streams. Every stream will add zero
 * blocks from the last data block to the submitted block index.
 */
static int submit_zero_block(struct blkhash *h)
{
    struct block *b;
    struct worker *w;
    int err;

    for (unsigned i = 0; i < h->config.streams; i++) {
        b = block_new(&h->streams[i], h->block_index, 0, NULL);
        if (b == NULL)
            return set_error(h, errno);

        w = worker_for_stream(h, i);
        err = worker_submit(w, b);
        if (err)
            return set_error(h, err);
    }

    h->update_index = h->block_index;
    return 0;
}

/*
 * Submit one data block to the worker handling this block.
 */
static int submit_data_block(struct blkhash *h, const void *buf, size_t len)
{
    struct stream *s;
    struct worker *w;
    struct block *b;
    int err;

    s = stream_for_block(h, h->block_index);
    w = worker_for_stream(h, s->id);

    b = block_new(s, h->block_index, len, buf);
    if (b == NULL)
        return set_error(h, errno);

    err = worker_submit(w, b);
    if (err)
        return set_error(h, err);

    h->update_index = h->block_index;
    h->block_index++;
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
    size_t n = MIN(len, h->config.block_size - h->pending_len);

    if (h->pending_zero) {
        /*
         * The buffer contains zeros, convert pending zeros to data.
         */
        memset(h->pending, 0, h->pending_len);
        h->pending_zero = false;
    }

    memcpy(h->pending + h->pending_len, buf, n);

    h->pending_len += n;

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
    size_t n = MIN(len, h->config.block_size - h->pending_len);

    if (h->pending_len == 0) {
        /* The buffer is empty, start collecting pending zeros. */
        h->pending_zero = true;
    } else if (!h->pending_zero) {
        /*
         * The buffer contains some data, convert the zeros to pending data.
         */
        memset(h->pending + h->pending_len, 0, n);
    }

    h->pending_len += n;

    return n;
}

/*
 * Consume count zero blocks, sending zero length block to all workers if
 * enough zero blocks were consumed since the last data block.
 */
static inline int consume_zero_blocks(struct blkhash *h, size_t count)
{
    const int64_t batch_size = STREAM_ZERO_BATCH_SIZE * h->config.streams;

    h->block_index += count;

    if (h->block_index - h->update_index >= batch_size)
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
static int consume_data_block(struct blkhash *h, const void *buf, size_t len)
{
    if (is_zero_block(h, buf, len)) {
        /* Fast path. */
        return consume_zero_blocks(h, 1);
    } else {
        /* Slow path. */
        return submit_data_block(h, buf, len);
    }
}

/*
 * Consume all pending data or zeros. The pending buffer may contain
 * full or partial block of data or zeros. The pending buffer is
 * cleared after this call.
 */
static int consume_pending(struct blkhash *h)
{
    assert(h->pending_len <= h->config.block_size);

    if (h->pending_len == h->config.block_size && h->pending_zero) {
        /* Fast path. */
        if (consume_zero_blocks(h, 1))
            return -1;
    } else {
        /*
         * Slow path if pending is partial block, fast path is pending
         * is full block and pending data is zeros.
         */
        if (h->pending_zero) {
            /* Convert partial block of zeros to data. */
            memset(h->pending, 0, h->pending_len);
        }

        if (consume_data_block(h, h->pending, h->pending_len))
            return -1;
    }

    h->pending_len = 0;
    h->pending_zero = false;
    return 0;
}

int blkhash_update(struct blkhash *h, const void *buf, size_t len)
{
    if (h->error)
        return h->error;

    /* Try to fill the pending buffer and consume it. */
    if (h->pending_len > 0) {
        size_t n = add_pending_data(h, buf, len);
        buf += n;
        len -= n;
        if (h->pending_len == h->config.block_size) {
            if (consume_pending(h))
                return h->error;
        }
    }

    /* Consume all full blocks in caller buffer. */
    while (len >= h->config.block_size) {
        if (consume_data_block(h, buf, h->config.block_size))
            return h->error;

        buf += h->config.block_size;
        len -= h->config.block_size;
    }

    /* Copy rest of the data to the internal buffer. */
    if (len > 0) {
        size_t n = add_pending_data(h, buf, len);
        buf += n;
        len -= n;
    }

    assert(len == 0);
    return 0;
}

int blkhash_zero(struct blkhash *h, size_t len)
{
    if (h->error)
        return h->error;

    /* Try to fill the pending buffer and consume it. */
    if (h->pending_len > 0) {
        len -= add_pending_zeros(h, len);
        if (h->pending_len == h->config.block_size) {
            if (consume_pending(h))
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
    if (len > 0) {
        len -= add_pending_zeros(h, len);
    }

    assert(len == 0);
    return 0;
}

static void stop_workers(struct blkhash *h)
{
    int err;

    /* Must use workers_count in case blkhash_new failed. */

    for (unsigned i = 0; i < h->workers_count; i++) {
        err = worker_stop(&h->workers[i]);
        if (err)
            set_error(h, err);
    }

    for (unsigned i = 0; i < h->workers_count; i++) {
        err = worker_join(&h->workers[i]);
        if (err)
            set_error(h, err);
    }
}

static int compute_root_hash(struct blkhash *h, unsigned char *md,
                             unsigned int *len)
{
    unsigned char stream_md[EVP_MAX_MD_SIZE];
    unsigned int stream_len;
    int err;

    for (unsigned i = 0; i < h->config.streams; i++) {
        err = stream_final(&h->streams[i], stream_md, &stream_len);
        if (err)
            return set_error(h, err);

        if (!EVP_DigestUpdate(h->root_ctx, stream_md, stream_len))
            return set_error(h, ENOMEM);
    }

    if (!EVP_DigestFinal_ex(h->root_ctx, md, len))
        return set_error(h, ENOMEM);

    return 0;
}

int blkhash_final(struct blkhash *h, unsigned char *md_value,
                  unsigned int *md_len)
{
    if (h->finalized)
        return EINVAL;

    h->finalized = true;

    if (h->pending_len > 0)
        consume_pending(h);

    if (h->error == 0)
        submit_zero_block(h);

    stop_workers(h);

    if (h->error == 0)
        compute_root_hash(h, md_value, md_len);

    return h->error;
}

void blkhash_free(struct blkhash *h)
{
    if (h == NULL)
        return;

    if (!h->finalized)
        stop_workers(h);

    /* Must use workers_count in case blkhash_new failed. */
    for (unsigned i = 0; i < h->workers_count; i++)
        worker_destroy(&h->workers[i]);

    /* Must use streams_count in case blkhash_new failed. */
    for (unsigned i = 0; i < h->streams_count; i++)
        stream_destroy(&h->streams[i]);

    free(h->pending);
    EVP_MD_CTX_free(h->root_ctx);
    free(h->workers);
    free(h->streams);

    free(h);
}
